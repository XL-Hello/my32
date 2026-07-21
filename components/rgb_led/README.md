# rgb_led 模块

`rgb_led` 是用于控制外接三色 LED 的应用层组件。模块以 LEDC PWM 驱动红、黄、绿三个颜色通道，向应用提供初始化、单色/混色、熄灯和流水灯接口。

该模块控制的是四线三色 LED（公共端 + R/Y/G 三个颜色端），不适用于板载 WS2812B 等单线时序灯珠。

## 功能

- 三路 8 bit PWM 亮度控制，单个通道范围为 `0` 至 `255`。
- 预置红、黄、绿、关闭四种颜色。
- 红 → 黄 → 绿循环流水灯。
- 防止普通颜色设置与流水灯任务同时写入硬件。
- 使用 `platform_log.h` 输出带函数名和行号的日志，日志标签为 `rgb_led`。

## 目录与分层

```text
components/rgb_led/
├── CMakeLists.txt
├── include/
│   ├── rgb_led.h           # 对 app 公开的接口
│   └── rgb_led_driver.h    # 组件内部驱动层接口
├── rgb_led.c               # 状态管理、颜色接口和流水灯任务
├── rgb_led_driver.c        # LEDC 初始化与三路 PWM 写入
└── README.md
```

应用代码只能包含 `rgb_led.h`；`rgb_led_driver.h` 是内部实现细节，不应被 `app/` 或其他组件直接使用。

## 硬件与 PWM 配置

当前配置位于 `rgb_led_driver.c`：

| 通道 | GPIO | LEDC 通道 | 默认用途 |
| --- | ---: | --- | --- |
| R | GPIO20 | `LEDC_CHANNEL_0` | 红色 |
| Y | GPIO21 | `LEDC_CHANNEL_1` | 黄色 |
| G | GPIO47 | `LEDC_CHANNEL_2` | 绿色 |

模块使用 `LEDC_LOW_SPEED_MODE`、`LEDC_TIMER_0`、`5 kHz` PWM 和 8 bit 分辨率。`RGB_LED_ACTIVE_LOW` 当前为 `false`，即高电平有效，适用于公共端接地的共阴极 LED。

每个颜色通道都必须串联限流电阻。若接入共阳极 LED，需要将公共端接至 `VDD33`，并把 `RGB_LED_ACTIVE_LOW` 改为 `true`，使低电平点亮。接线前应确认 LED 公共端类型；高亮 LED、灯带或并联灯珠应由三极管或 MOSFET 驱动，不能直接由 GPIO 供电。

## 公开接口

```c
esp_err_t rgb_led_init(void);
esp_err_t rgb_led_off(void);
esp_err_t rgb_led_set_rgb(uint8_t red, uint8_t yellow, uint8_t green);
esp_err_t rgb_led_set_color(rgb_led_color_t color);
esp_err_t rgb_led_start_chase(uint32_t interval_ms);
esp_err_t rgb_led_stop_chase(void);
```

### `rgb_led_init`

初始化 LEDC 定时器和三个 PWM 通道，并创建流水灯停止同步对象。初始化成功后 LED 会处于全灭状态。该函数可重复调用，重复调用直接返回 `ESP_OK`。

### `rgb_led_set_rgb`

设置三个通道的亮度。参数分别对应红、黄、绿通道，范围均为 `0` 至 `255`。例如：

```c
ESP_ERROR_CHECK(rgb_led_set_rgb(255, 64, 0));
```

未初始化时返回 `ESP_ERR_INVALID_STATE`；流水灯运行时也返回 `ESP_ERR_INVALID_STATE`，避免多个执行上下文竞争 PWM 输出。

### `rgb_led_set_color`

设置预置颜色：

```c
ESP_ERROR_CHECK(rgb_led_set_color(RGB_LED_COLOR_RED));
ESP_ERROR_CHECK(rgb_led_set_color(RGB_LED_COLOR_YELLOW));
ESP_ERROR_CHECK(rgb_led_set_color(RGB_LED_COLOR_GREEN));
ESP_ERROR_CHECK(rgb_led_set_color(RGB_LED_COLOR_OFF));
```

无效枚举值返回 `ESP_ERR_INVALID_ARG`。

### `rgb_led_off`

将三个通道亮度都设为 `0`。其状态限制与 `rgb_led_set_rgb()` 相同，因此不能在流水灯运行期间直接调用。

### `rgb_led_start_chase`

创建唯一的 FreeRTOS 任务，按红、黄、绿顺序循环显示。`interval_ms` 是每种颜色的保持时间，必须大于零。重复启动或未初始化时返回 `ESP_ERR_INVALID_STATE`；任务创建失败时返回 `ESP_ERR_NO_MEM`。

### `rgb_led_stop_chase`

向流水灯任务发送停止通知，最多等待 1000 ms，任务退出后自动熄灯。未启动流水灯时返回 `ESP_ERR_INVALID_STATE`；等待超时时返回 `ESP_ERR_TIMEOUT`。

## 状态关系

```text
未初始化 --rgb_led_init()--> 就绪/熄灯
就绪 ------设置颜色或亮度-----> 就绪
就绪 ------start_chase()------> 流水灯运行
流水灯运行 --stop_chase()-----> 就绪/熄灯
```

流水灯运行期间，`rgb_led_set_rgb()`、`rgb_led_set_color()` 和 `rgb_led_off()` 不可用；应先调用 `rgb_led_stop_chase()`。

## 在应用中使用

`app/CMakeLists.txt` 已通过 `REQUIRES rgb_led` 引入组件。应用代码示例：

```c
#include "rgb_led.h"

void app_main(void)
{
    ESP_ERROR_CHECK(rgb_led_init());
    ESP_ERROR_CHECK(rgb_led_start_chase(500));

    /* 需要手动控制颜色前，先停止流水灯。 */
    ESP_ERROR_CHECK(rgb_led_stop_chase());
    ESP_ERROR_CHECK(rgb_led_set_color(RGB_LED_COLOR_GREEN));
}
```

使用项目提供的构建脚本编译和烧录：

```bash
./build.sh build
./build.sh flash /dev/ttyACM0
```

## 日志与故障排查

模块会输出 LEDC 初始化、颜色设置、流水灯启动、颜色切换和停止等日志。日志格式由 `platform_log.h` 统一封装，包含标签、函数名和源码行号。

若 LED 不亮或颜色异常，按以下顺序检查：

1. 确认 LED 公共端类型与 `RGB_LED_ACTIVE_LOW` 配置匹配。
2. 检查 R/Y/G 三个引脚是否依次连接 GPIO20/GPIO21/GPIO47，且每路都有独立限流电阻。
3. 查看串口中是否出现 `LEDC PWM initialized` 和 `RGB LED initialized`。
4. 使用 `rgb_led_set_color()` 分别验证三个单色通道，再验证流水灯。
