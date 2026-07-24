#ifndef _SIM_UART_H_
#define _SIM_UART_H_

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

// ===================== 配置区 =====================
#define SIM_UART_TX_PIN    GPIO_NUM_9
#define SIM_UART_RX_PIN    GPIO_NUM_11

#define SIM_UART_BAUDRATE  9600

// 计算每位持续时间(us)
#define SIM_UART_BIT_US          104
#define SIM_UART_BIT_US_HALF     52

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
