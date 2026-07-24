#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hal/gpio_ll.h"
#include "sim_uart.h"
#include "soc/gpio_struct.h"
#include <string.h>

typedef enum
{
    TX_IDLE,
    TX_START,
    TX_DATA,
    TX_STOP,
}tx_state_t;

typedef enum
{
    RX_IDLE,
    RX_START,
    RX_DATA,
    RX_STOP,
}rx_state_t;

typedef struct
{
    volatile tx_state_t state;
    uint8_t data;
    uint8_t bit;
}soft_tx_t;

typedef struct
{
    volatile rx_state_t state;
    uint8_t data;
    uint8_t bit;
    uint8_t half_tick;
}soft_rx_t;

static soft_tx_t s_tx;
static soft_rx_t s_rx;
static RingbufHandle_t s_rx_buffer;
static gptimer_handle_t s_rx_gptimer;
static gptimer_handle_t s_tx_gptimer;
static SemaphoreHandle_t s_tx_mutex;
static uint32_t s_gpio_isr_core;

static void sim_uart_isr_handler(void *arg);
static esp_err_t sim_uart_tx_timer_init(void);

static gptimer_alarm_config_t s_half_bit_alarm = {
    .alarm_count = SIM_UART_BIT_US_HALF,
    .reload_count = 0,
    .flags.auto_reload_on_alarm = true,
};

static gptimer_alarm_config_t s_tx_bit_alarm = {
    .alarm_count = SIM_UART_BIT_US,
    .reload_count = 0,
    .flags.auto_reload_on_alarm = true,
};

// 每个 alarm 推进一个 TX bit，发送任务只装载单字节并等待状态恢复为空闲。
static bool IRAM_ATTR tx_timer_on_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    (void)edata;
    (void)user_ctx;

    if (s_tx.state == TX_START) {
        s_tx.bit = 0;
        gpio_ll_set_level(&GPIO, SIM_UART_TX_PIN, 0);
        s_tx.state = TX_DATA;
        return false;
    }
    else if (s_tx.state == TX_DATA) {
        gpio_ll_set_level(&GPIO, SIM_UART_TX_PIN, (s_tx.data >> s_tx.bit) & 1U);
        s_tx.bit++;
        if (s_tx.bit >= SIM_UART_DATA_BITS) {
            s_tx.state = TX_STOP;
        }
        return false;
    }
    else if (s_tx.state == TX_STOP) {
        gpio_ll_set_level(&GPIO, SIM_UART_TX_PIN, 1);
        gptimer_stop(timer);
        s_tx.state = TX_IDLE;
    }

    return false;
}

// 定时器每 0.5 bit 中断一次；仅在奇数半位采样，避免在 ISR 内动态修改绝对告警值。
static bool IRAM_ATTR timer_on_alarm_cb(gptimer_handle_t timer,
                                        const gptimer_alarm_event_data_t *edata,
                                        void *user_ctx)
{
    (void)edata;
    (void)user_ctx;

    s_rx.half_tick++;
    if ((s_rx.half_tick & 1U) == 0) {
        return false;
    }

    // 采样起始位，高->毛刺；低->起始位；
    if (s_rx.state == RX_START) {
        if (gpio_ll_get_level(&GPIO, SIM_UART_RX_PIN) != 0) {
            s_rx.state = RX_IDLE;
            gptimer_stop(timer);
            gpio_ll_clear_intr_status(&GPIO, 1U << SIM_UART_RX_PIN);
            gpio_ll_intr_enable_on_core(&GPIO, s_gpio_isr_core, SIM_UART_RX_PIN);
            return false;
        }
        // 已确认起始位；下一个奇数半位（1.5 bit）开始采集数据位。
        s_rx.state = RX_DATA;
    } else if (s_rx.state == RX_DATA) {//运行8bit
        if (gpio_ll_get_level(&GPIO, SIM_UART_RX_PIN)) {
            s_rx.data |= (uint8_t)(1U << s_rx.bit);
        }
        s_rx.bit++;
        if (s_rx.bit >= SIM_UART_DATA_BITS) {
            s_rx.state = RX_STOP;
        }
    } else if (s_rx.state == RX_STOP) {//运行1bit
        if (gpio_ll_get_level(&GPIO, SIM_UART_RX_PIN)) {
            xRingbufferSendFromISR(s_rx_buffer, &s_rx.data, sizeof(s_rx.data), NULL);
        }
        s_rx.state = RX_IDLE;
        gptimer_stop(timer);
        gpio_ll_clear_intr_status(&GPIO, 1U << SIM_UART_RX_PIN);
        gpio_ll_intr_enable_on_core(&GPIO, s_gpio_isr_core, SIM_UART_RX_PIN);
    }

    return false;
}

// 初始化软件 UART 使用的硬件定时器：计数单位为 1us，一个 alarm 周期对应一个 bit。
static esp_err_t sim_uart_timer_init(void)
{
    // 已创建并启用时直接复用，避免重复申请硬件定时器资源。
    if (s_rx_gptimer != NULL) {
        return ESP_OK;
    }

    const gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT, // 使用驱动选择的默认时钟源。
        .direction = GPTIMER_COUNT_UP,      // 计数器从 0 开始向上递增。
        .resolution_hz = 1000000UL,         // 计数频率为 1 MHz，即 1 个计数值等于 1us。
        .intr_priority = 0,                 // 由中断分配器选择可用的低/中优先级向量。
    };
    // 创建 GPTimer，并将句柄保存到 s_gptimer 供后续 alarm/启动接口使用。
    esp_err_t err = gptimer_new_timer(&timer_config, &s_rx_gptimer);
    if (err != ESP_OK) {
        return err;
    }

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_on_alarm_cb, // 注册定时器 alarm 回调函数，处理收发状态机的计时逻辑。
    };
    err = gptimer_register_event_callbacks(s_rx_gptimer, &cbs, NULL);
    if (err != ESP_OK) {
        gptimer_del_timer(s_rx_gptimer);
        s_rx_gptimer = NULL;
        return err;
    }

    err = gptimer_set_alarm_action(s_rx_gptimer, &s_half_bit_alarm);
    if (err != ESP_OK) {
        gptimer_del_timer(s_rx_gptimer);
        s_rx_gptimer = NULL;
        return err;
    }

    // 将定时器从 init 状态切换到 enable 状态；此处不启动计数。
    err = gptimer_enable(s_rx_gptimer);
    if (err != ESP_OK) {
        // 启用失败时同样释放句柄，保持初始化失败后的状态一致。
        gptimer_del_timer(s_rx_gptimer);
        s_rx_gptimer = NULL;
    }

    return err;
}

static esp_err_t sim_uart_tx_timer_init(void)
{
    if (s_tx_gptimer != NULL) {
        return ESP_OK;
    }

    const gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000UL,
        .intr_priority = 0,
    };
    esp_err_t err = gptimer_new_timer(&timer_config, &s_tx_gptimer);
    if (err != ESP_OK) {
        return err;
    }

    const gptimer_event_callbacks_t cbs = {
        .on_alarm = tx_timer_on_alarm_cb,
    };
    err = gptimer_register_event_callbacks(s_tx_gptimer, &cbs, NULL);
    if (err == ESP_OK) {
        err = gptimer_set_alarm_action(s_tx_gptimer, &s_tx_bit_alarm);
    }
    if (err == ESP_OK) {
        err = gptimer_enable(s_tx_gptimer);
    }
    if (err != ESP_OK) {
        gptimer_del_timer(s_tx_gptimer);
        s_tx_gptimer = NULL;
    }
    return err;
}

int sim_uart_init(void)
{

    // 初始化定时器
    if (sim_uart_timer_init() != ESP_OK) {
        return -1;
    }
    if (sim_uart_tx_timer_init() != ESP_OK) {
        return -1;
    }

    if (s_tx_mutex == NULL) {
        s_tx_mutex = xSemaphoreCreateMutex();
    }
    if (s_tx_mutex == NULL) {
        return -1;
    }

    // RX 路径使用 ESP-IDF 的字节型 Ring Buffer，支持中断生产、任务消费。
    if (s_rx_buffer == NULL) {
        s_rx_buffer = xRingbufferCreate(SIM_UART_RX_BUF_LEN, RINGBUF_TYPE_BYTEBUF);
        if (s_rx_buffer == NULL) {
            return -1;
        }
    }

    //初始化管脚
    gpio_config_t tx_config = {
        .pin_bit_mask = (1ULL << SIM_UART_TX_PIN),
        .mode = GPIO_MODE_OUTPUT,//普通输出模式
        .pull_up_en = GPIO_PULLUP_ENABLE,//上拉使能
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    if (gpio_config(&tx_config) != ESP_OK) {
        return -1;
    }
    gpio_set_level(SIM_UART_TX_PIN, 1);//默认高电平，空闲状态

    gpio_config_t rx_config = {
        .pin_bit_mask = (1ULL << SIM_UART_RX_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,//上拉使能
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    if (gpio_config(&rx_config) != ESP_OK) {
        return -1;
    }

    // 不强制指定中断级别，避免和系统已使用的向量发生资源冲突。
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return -1;
    }
    s_gpio_isr_core = xPortGetCoreID();
    if (gpio_isr_handler_add(SIM_UART_RX_PIN, sim_uart_isr_handler, NULL) != ESP_OK ||
        gpio_set_intr_type(SIM_UART_RX_PIN, GPIO_INTR_NEGEDGE) != ESP_OK ||
        gpio_intr_enable(SIM_UART_RX_PIN) != ESP_OK) {
        return -1;
    }
 
    return 0;
}

int sim_uart_send(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0 || s_tx_gptimer == NULL || s_tx_mutex == NULL) {
        return -1;
    }

    if (xSemaphoreTake(s_tx_mutex, portMAX_DELAY) != pdTRUE) {
        return -1;
    }

    int result = 0;
    for (size_t i = 0; i < len; i++) {
        s_tx.data = data[i];
        s_tx.state = TX_START;

        // 与 RX 起始沿处理一致：每个字节发送前清零计数器并启动周期定时器。
        esp_err_t err = gptimer_set_raw_count(s_tx_gptimer, 0);
        if (err == ESP_OK) {
            err = gptimer_start(s_tx_gptimer);
        }
        if (err != ESP_OK) {
            s_tx.state = TX_IDLE;
            gpio_set_level(SIM_UART_TX_PIN, 1);
            result = -1;
            break;
        }

        // TX ISR 完成停止位后恢复 TX_IDLE，再继续发送下一个字节。
        while (s_tx.state != TX_IDLE) {
            taskYIELD();
        }
    }

    xSemaphoreGive(s_tx_mutex);
    return result;
}

int sim_uart_recv(uint8_t *data, size_t len)
{
    if (!data || len == 0) {
        return -1;
    }

    if (s_rx_buffer == NULL) {
        return -1;
    }

    size_t read_len = 0;
    while (read_len < len) {
        size_t item_len;
        uint8_t *item = xRingbufferReceiveUpTo(s_rx_buffer, &item_len, 0, len - read_len);
        if (item == NULL) {
            break;
        }

        memcpy(data + read_len, item, item_len);
        read_len += item_len;
        vRingbufferReturnItem(s_rx_buffer, item);
    }

    return (int)read_len;
}


// 中断服务函数：捕获起始位后读取整帧
static void IRAM_ATTR sim_uart_isr_handler(void *arg)
{
    (void)arg;
    if (s_rx.state == RX_IDLE) {
        // 捕获到起始位下降沿，禁用 RX 中断
        gpio_ll_intr_disable(&GPIO, SIM_UART_RX_PIN);
        //更新状态机，重置数据
        s_rx.state = RX_START;
        s_rx.bit = 0;
        s_rx.data = 0;
        s_rx.half_tick = 0;
        // 启动固定 0.5bit 周期定时器；奇数半位采样，偶数半位跳过。
        gptimer_set_raw_count(s_rx_gptimer, 0);
        gptimer_start(s_rx_gptimer);
    }
}
