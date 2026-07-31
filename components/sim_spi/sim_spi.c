#include <string.h>

#include "driver/gptimer.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_ll.h"
#include "sim_spi.h"
#include "soc/gpio_struct.h"

static int sim_spi_gpio_init(gpio_num_t pin, gpio_mode_t mode, gpio_pullup_t pull_up,
                             gpio_pulldown_t pull_down, gpio_int_type_t intr_type, int level)
{
    const gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << pin),
        .mode = mode,
        .pull_up_en = pull_up,
        .pull_down_en = pull_down,
        .intr_type = intr_type,
    };
    if (gpio_config(&io_config) != ESP_OK) {
        return -1;
    }

    if (level >= 0) {
        gpio_set_level(pin, level);
    }
    return ESP_OK;
}

// 发送当前字节的 bit_index 位到 MOSI。
static void IRAM_ATTR sim_spi_drive_tx_bit(sim_spi_config_t *spi_config)
{
    uint8_t tx_byte = 0;

    if (spi_config->tx_buf != NULL && spi_config->byte_index < spi_config->tx_len) {
        tx_byte = spi_config->tx_buf[spi_config->byte_index];
    }
    spi_config->tx_byte = tx_byte;
    gpio_ll_set_level(&GPIO, spi_config->mosi_pin,
                      (tx_byte >> (7U - spi_config->bit_index)) & 1U);
}

static bool IRAM_ATTR timer_on_spi_alarm_cb(gptimer_handle_t timer,
                                            const gptimer_alarm_event_data_t *edata,
                                            void *user_ctx)
{
    (void)edata;

    sim_spi_config_t *spi_config = (sim_spi_config_t *)user_ctx;
    if (spi_config == NULL) {
        gptimer_stop(timer);
        return false;
    }

    switch (spi_config->state) {
        case SIM_SPI_STATE_START:
            // 在首个时钟沿前完成 CS 建立时间，并将 SCK 置于 CPOL 所规定的空闲电平。
            gpio_ll_set_level(&GPIO, spi_config->cs_pin, 0);
            gpio_ll_set_level(&GPIO, spi_config->sck_pin, spi_config->idle_level);
            spi_config->sck_level = spi_config->idle_level;
            spi_config->rx_byte = 0;
            spi_config->bit_index = 0;

            // CPHA=0 时首位必须在第一个采样沿到来前有效。
            if ((spi_config->mode & 0x01U) == 0U) {
                sim_spi_drive_tx_bit(spi_config);
            }
            spi_config->state = SIM_SPI_STATE_DATA;
            break;

        case SIM_SPI_STATE_DATA: {
            // 每次 alarm 产生一个半位时钟边沿；该边沿不是采样 MISO，就是更新 MOSI。
            spi_config->sck_level ^= 1;
            gpio_ll_set_level(&GPIO, spi_config->sck_pin, spi_config->sck_level);

            const gpio_int_type_t current_edge = spi_config->sck_level ?
                                                 GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE;
            if (current_edge == spi_config->sample_edge) {
                spi_config->rx_byte = (uint8_t)((spi_config->rx_byte << 1U) |
                                                gpio_ll_get_level(&GPIO, spi_config->miso_pin));
                spi_config->bit_index++;

                if (spi_config->bit_index == 8U) {
                    if (spi_config->rx_buf != NULL && spi_config->byte_index < spi_config->rx_len) {
                        spi_config->rx_buf[spi_config->byte_index] = spi_config->rx_byte;
                    }

                    spi_config->byte_index++;
                    if (spi_config->byte_index >= spi_config->tx_len) {
                        // 末位采样后，若 SCK 未处于空闲电平，先补齐返回空闲的时钟沿，
                        // 再延迟半位周期撤销 CS，以满足末位保持时间。
                        spi_config->state = (spi_config->sck_level == spi_config->idle_level) ?
                                            SIM_SPI_STATE_STOP : SIM_SPI_STATE_FINISH_CLOCK;
                    } else {
                        spi_config->bit_index = 0;
                        spi_config->rx_byte = 0;
                    }
                }
            } else {
                // CPHA=1 的首位及所有后续数据位均在非采样沿更新。
                sim_spi_drive_tx_bit(spi_config);
            }
            break;
        }

        case SIM_SPI_STATE_FINISH_CLOCK:
            gpio_ll_set_level(&GPIO, spi_config->sck_pin, spi_config->idle_level);
            spi_config->sck_level = spi_config->idle_level;
            spi_config->state = SIM_SPI_STATE_STOP;
            break;

        case SIM_SPI_STATE_STOP:
            gpio_ll_set_level(&GPIO, spi_config->sck_pin, spi_config->idle_level);
            gpio_ll_set_level(&GPIO, spi_config->cs_pin, 1);
            gptimer_stop(timer);
            spi_config->state = SIM_SPI_STATE_IDLE;
            break;

        case SIM_SPI_STATE_IDLE:
            // gptimer_stop() 后可能仍有一个已经挂起的 alarm 回调。此时 timer 已经停止，
            // 直接返回，避免重复调用 gptimer_stop() 产生“timer is not running”日志。
            break;

        default:
            // 防御性处理：非法状态下确保总线回到安全空闲状态并停止 timer。
            gpio_ll_set_level(&GPIO, spi_config->sck_pin, spi_config->idle_level);
            gpio_ll_set_level(&GPIO, spi_config->cs_pin, 1);
            gptimer_stop(timer);
            spi_config->state = SIM_SPI_STATE_IDLE;
            break;
    }

    return false;
}

static esp_err_t sim_spi_timer_init(sim_spi_config_t *spi_config, uint32_t half_bit_period_ticks)
{
    if (spi_config == NULL || half_bit_period_ticks == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = SIM_SPI_TIMER_RESOLUTION_HZ,
        .intr_priority = 0,
    };

    esp_err_t err = gptimer_new_timer(&timer_config, &spi_config->timer_handle);
    if (err != ESP_OK) {
        return err;
    }

    const gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_on_spi_alarm_cb,
    };
    err = gptimer_register_event_callbacks(spi_config->timer_handle, &cbs, spi_config);
    if (err != ESP_OK) {
        goto cleanup;
    }

    // 周期性半位 alarm：状态机每次回调只处理一个时钟边沿，传输完成后在回调中停止 timer。
    const gptimer_alarm_config_t half_bit_alarm = {
        .alarm_count = half_bit_period_ticks,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    err = gptimer_set_alarm_action(spi_config->timer_handle, &half_bit_alarm);
    if (err != ESP_OK) {
        goto cleanup;
    }

    err = gptimer_enable(spi_config->timer_handle);
    if (err != ESP_OK) {
        goto cleanup;
    }
    return ESP_OK;

cleanup:
    gptimer_del_timer(spi_config->timer_handle);
    spi_config->timer_handle = NULL;
    return err;
}

int sim_spi_init(sim_spi_config_t *spi_config)
{
    if (spi_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(spi_config, 0, sizeof(*spi_config));
    // W25Q64 接线：GPIO35-D1(MOSI)，GPIO36-SLK(SCK)，GPIO37-D0(MISO)，GPIO38-CS。
    spi_config->sck_pin = GPIO_NUM_36;
    spi_config->mosi_pin = GPIO_NUM_35;
    spi_config->miso_pin = GPIO_NUM_37;
    spi_config->cs_pin = GPIO_NUM_38;
    spi_config->mode = SIM_SPI_MODE_0;
    spi_config->baudrate = 50000;

    spi_config->idle_level = (spi_config->mode & 0x02U) ? 1 : 0;
    // CPHA=0 在首个（leading）边沿采样；CPHA=1 在第二个（trailing）边沿采样。
    spi_config->sample_edge = ((spi_config->mode & 0x01U) == 0U) ?
                              (spi_config->idle_level ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE) :
                              (spi_config->idle_level ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE);
    spi_config->sck_level = spi_config->idle_level;
    spi_config->state = SIM_SPI_STATE_IDLE;

    // CS 始终空闲为高，MOSI 空闲为低；MISO 由 timer ISR 轮询采样，不启用 GPIO 中断。
    if (sim_spi_gpio_init(spi_config->sck_pin, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE,
                          GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE, spi_config->idle_level) != ESP_OK ||
        sim_spi_gpio_init(spi_config->mosi_pin, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE,
                          GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE, 0) != ESP_OK ||
        sim_spi_gpio_init(spi_config->miso_pin, GPIO_MODE_INPUT, GPIO_PULLUP_DISABLE,
                          GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE, -1) != ESP_OK ||
        sim_spi_gpio_init(spi_config->cs_pin, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE,
                          GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE, 1) != ESP_OK) {
        return ESP_FAIL;
    }

    // 半周期最小为 1us，故 1MHz GPTimer 最多可模拟 500kHz SCK。
    if (spi_config->baudrate == 0 || spi_config->baudrate > SIM_SPI_TIMER_RESOLUTION_HZ / 2U) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint32_t period = (SIM_SPI_TIMER_RESOLUTION_HZ + spi_config->baudrate) /
                            (2U * spi_config->baudrate);
    const esp_err_t err = sim_spi_timer_init(spi_config, period);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t sim_spi_deinit(sim_spi_config_t *spi_config)
{
    if (spi_config == NULL || spi_config->timer_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (spi_config->state != SIM_SPI_STATE_IDLE) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = gptimer_disable(spi_config->timer_handle);
    if (err != ESP_OK) {
        return err;
    }
    err = gptimer_del_timer(spi_config->timer_handle);
    if (err != ESP_OK) {
        return err;
    }

    memset(spi_config, 0, sizeof(*spi_config));
    return ESP_OK;
}

static esp_err_t sim_spi_transfer_internal(sim_spi_config_t *spi_config, const uint8_t *tx,
                                           uint8_t *rx, uint16_t len)
{
    if (spi_config == NULL || spi_config->timer_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len == 0) {
        return ESP_OK;
    }
    spi_config->tx_buf = (uint8_t *)tx;
    spi_config->rx_buf = rx;
    spi_config->tx_len = len;
    spi_config->rx_len = len;
    spi_config->byte_index = 0;
    spi_config->tx_byte = 0;
    spi_config->rx_byte = 0;
    spi_config->bit_index = 0;
    spi_config->sck_level = spi_config->idle_level;
    spi_config->state = SIM_SPI_STATE_START;
    gpio_set_level(spi_config->sck_pin, spi_config->idle_level);
    gpio_set_level(spi_config->cs_pin, 1);

    esp_err_t err = gptimer_set_raw_count(spi_config->timer_handle, 0);
    if (err == ESP_OK) {
        err = gptimer_start(spi_config->timer_handle);
    }
    if (err != ESP_OK) {
        spi_config->state = SIM_SPI_STATE_IDLE;
        gpio_set_level(spi_config->sck_pin, spi_config->idle_level);
        gpio_set_level(spi_config->cs_pin, 1);
        return err;
    }

    // timer ISR 在 STOP 状态停止 timer 并最后写入 IDLE；state 为 volatile，避免被编译器缓存。
    while (spi_config->state != SIM_SPI_STATE_IDLE) {
        taskYIELD();
    }
    return ESP_OK;
}

void sim_spi_transfer(sim_spi_config_t *spi_config, uint8_t *tx, uint8_t *rx, uint16_t len)
{
    (void)sim_spi_transfer_internal(spi_config, tx, rx, len);
}

int sim_spi_send(sim_spi_config_t *spi_config, const uint8_t *data, size_t len)
{
    if (len > UINT16_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }
    return sim_spi_transfer_internal(spi_config, data, NULL, (uint16_t)len);
}
