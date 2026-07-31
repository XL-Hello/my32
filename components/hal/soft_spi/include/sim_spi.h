#ifndef _SIM_SPI_H_
#define _SIM_SPI_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"

typedef enum {
    SIM_SPI_MODE_0 = 0, // CPOL=0, CPHA=0
    SIM_SPI_MODE_1,     // CPOL=0, CPHA=1
    SIM_SPI_MODE_2,     // CPOL=1, CPHA=0
    SIM_SPI_MODE_3      // CPOL=1, CPHA=1
} sim_spi_mode_t;

typedef enum {
    SIM_SPI_STATE_IDLE,
    SIM_SPI_STATE_START,
    SIM_SPI_STATE_DATA,
    // CPHA=0 的最后一个采样沿后，需要补齐一个边沿使 SCK 回到空闲电平。
    SIM_SPI_STATE_FINISH_CLOCK,
    SIM_SPI_STATE_STOP,
} sim_spi_state_t;

typedef struct {
    gpio_num_t sck_pin;
    gpio_num_t mosi_pin;
    gpio_num_t miso_pin;
    gpio_num_t cs_pin;

    sim_spi_mode_t mode;
    int idle_level;                // CPOL，SCK 空闲电平
    gpio_int_type_t sample_edge;   // 逻辑采样沿，不启用 GPIO 中断
    volatile int sck_level;
    uint32_t baudrate;

    gptimer_handle_t timer_handle;
    volatile sim_spi_state_t state;

    uint8_t *tx_buf;
    size_t tx_len;
    uint8_t *rx_buf;
    size_t rx_len;

    volatile size_t byte_index;
    volatile uint8_t tx_byte;
    volatile uint8_t rx_byte;
    volatile uint8_t bit_index;
} sim_spi_config_t;

// GPTimer 以 1 MHz 运行，1 tick 等于 1 us。
#define SIM_SPI_TIMER_RESOLUTION_HZ 1000000UL

int sim_spi_init(sim_spi_config_t *spi_config);

/**
 * @brief 释放软件 SPI 使用的 GPTimer。
 *
 * 仅可在没有正在进行的传输时调用。
 */
esp_err_t sim_spi_deinit(sim_spi_config_t *spi_config);

/**
 * @brief 以软件 SPI 进行同步、全双工传输。
 *
 * tx 或 rx 可以为 NULL：tx 为 NULL 时发送 0x00，rx 为 NULL 时丢弃接收数据。
 * 函数在最后一个时钟沿和 CS 去使能完成后返回。该实现不可重入，仅支持单任务调用。
 */
void sim_spi_transfer(sim_spi_config_t *spi_config, uint8_t *tx, uint8_t *rx, uint16_t len);

#endif // _SIM_SPI_H_
