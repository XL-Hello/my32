# WS2812B 组件

本组件使用项目内 `components/ESP/led_strip` 驱动 WS2812B 灯珠，对外接口不依赖后端。默认使用 RMT；将 CMake 配置 `WS2812B_BACKEND` 设为 `SPI` 后，改用 `led_strip` SPI 后端。`led_strip` v2.5.5 源码已随项目保存，构建不依赖 Component Manager 或网络注册表。

## 配置

在组件的编译定义中可覆盖以下宏：

| 宏 | 默认值 | 说明 |
| --- | --- | --- |
| `WS2812B_BACKEND` | `RMT` | CMake 配置：`RMT` 或 `SPI`，最终定义对应的 C 宏 |
| `WS2812B_GPIO_NUM` | `48` | WS2812B DIN 数据线 GPIO |
| `WS2812B_LED_COUNT` | `1` | 串联灯珠数量 |
| `WS2812B_RMT_RESOLUTION_HZ` | `10000000` | RMT 时钟分辨率 |

SPI 后端固定使用 `SPI2_HOST`，且该 SPI 总线不能再被其他设备使用；实际输出只使用 MOSI，经 GPIO 矩阵映射至 `WS2812B_GPIO_NUM`。

切换至 SPI 后端并重新配置：

```bash
source ./build.sh env
idf.py -DWS2812B_BACKEND=SPI reconfigure
./build.sh build
```

切回默认 RMT：

```bash
idf.py -DWS2812B_BACKEND=RMT reconfigure
```

## 使用

```c
#include "ws2812b.h"

ESP_ERROR_CHECK(ws2812b_init());
ESP_ERROR_CHECK(ws2812b_set_pixel(0, 255, 0, 0));
ESP_ERROR_CHECK(ws2812b_refresh());
```

`ws2812b_set_pixel()` 仅更新内部帧缓存；调用 `ws2812b_refresh()` 后才会发送数据。`ws2812b_set_all()` 和 `ws2812b_clear()` 会立即刷新。
