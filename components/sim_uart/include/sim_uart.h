#ifndef _SIM_UART_H_
#define _SIM_UART_H_

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

// ===================== 配置区 =====================
#define SIM_UART_TX_PIN    GPIO_NUM_9
#define SIM_UART_RX_PIN    GPIO_NUM_11

#define SIM_UART_BAUDRATE  9600U

// GPTimer 以 1 MHz 运行，1 tick 等于 1 us。半位周期按最接近的整数 us
// 计算，整位固定为两个半位，保证 TX 和 RX 使用一致的时间基准。
#define SIM_UART_TIMER_RESOLUTION_HZ 1000000UL

#if SIM_UART_BAUDRATE == 0
#error "SIM_UART_BAUDRATE must be greater than zero"
#endif

#define SIM_UART_BIT_US_HALF \
    ((SIM_UART_TIMER_RESOLUTION_HZ + SIM_UART_BAUDRATE) / (2UL * SIM_UART_BAUDRATE))
#define SIM_UART_BIT_US      (2UL * SIM_UART_BIT_US_HALF)

_Static_assert(SIM_UART_BIT_US_HALF > 0,
               "SIM_UART_BAUDRATE is too high for 1 us timer resolution");

#define SIM_UART_DATA_BITS   8
#define SIM_UART_CHECK_BITS  0 // 0:stop 1:even 2:odd
#define SIM_UART_STOP_BITS   1

#define SIM_UART_RX_BUF_LEN 128
// ==================================================

int sim_uart_init(void);

int sim_uart_send(const uint8_t *data, size_t len);

int sim_uart_recv(uint8_t *data, size_t len);


void sim_uart_test_init(void);
#endif // _SIM_UART_H_
