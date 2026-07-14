# YD-ESP32-S3-COREBOARD 硬件管脚连接

本文根据 `YD-ESP32-S3-SCH.pdf` 重新整理。原理图标题为 `YD-ESP32-S3-COREBOARD`，日期 `2022/6/1`。网络名和器件标号以该 PDF 为准。

## 1. 电源

| 模块/器件 | 原理图标号 | 连接 | 说明 |
| --- | --- | --- | --- |
| USB1 Type-C 下载口 | USB1 | VBUS 网络 `VBUSB`，经 D1 接入 `5V` | USB1 可给板上 5 V 供电 |
| USB2 Type-C 原生 USB 口 | USB2 | VBUS 经 D2 接入 `5V` | USB2 可给板上 5 V 供电 |
| 外部 5 V 输入/输出 | J1-21 | J1-21 节点经 D3 到 `5V`，并有 `IN-OUT` 0 ohm/0603 焊接位旁路 | 可作为外部 5 V 输入/输出节点，是否直连取决于焊接位 |
| 3.3 V LDO | U3 AMS1117-3.3 | `5V` -> U3-3 IN，U3-2/4 OUT/TAB -> `VDD33`，U3-1 GND | 板上 3.3 V 电源 |
| LDO 输入电容 | C4 | `5V` 到 GND | 10 uF/A |
| LDO 输出电容 | C3 | `VDD33` 到 GND | 22 uF/A |
| 3.3 V 去耦 | C5/C6 | `VDD33` 到 GND | C5 10 uF/0402，C6 0.1 uF/0402 |
| 电源指示灯 | PWR + R6 | `VDD33` 经 R6 到红色 LED 再到 GND | R6 为 680 ohm/0402 |
| ESP32-S3 模组电源 | U4 | U4-2 `3V3` 接 `VDD33`，U4-1/40/EPAD 接 GND | ESP32-S3-WROOM-1 |

## 2. ESP32-S3-WROOM-1 模组 U4

| U4 脚号 | 模组脚名 | 原理图网络 |
| --- | --- | --- |
| 1 | GND1 | GND |
| 2 | 3V3 | VDD33 |
| 3 | EN | CHIP_PU |
| 4 | IO4 | GPIO4 |
| 5 | IO5 | GPIO5 |
| 6 | IO6 | GPIO6 |
| 7 | IO7 | GPIO7 |
| 8 | IO15 | GPIO15 |
| 9 | IO16 | GPIO16 |
| 10 | IO17 | GPIO17 |
| 11 | IO18 | GPIO18 |
| 12 | IO8 | GPIO8 |
| 13 | IO19 | GPIO19 |
| 14 | IO20 | GPIO20 |
| 15 | IO3 | GPIO3 |
| 16 | IO46 | GPIO46 |
| 17 | IO9 | GPIO9 |
| 18 | IO10 | GPIO10 |
| 19 | IO11 | GPIO11 |
| 20 | IO12 | GPIO12 |
| 21 | IO13 | GPIO13 |
| 22 | IO14 | GPIO14 |
| 23 | IO21 | GPIO21 |
| 24 | IO47 | GPIO47 |
| 25 | IO48 | GPIO48 |
| 26 | IO45 | GPIO45 |
| 27 | IO0 | GPIO0 |
| 28 | IO35 | GPIO35 |
| 29 | IO36 | GPIO36 |
| 30 | IO37 | GPIO37 |
| 31 | IO38 | GPIO38 |
| 32 | IO39 | GPIO39 |
| 33 | IO40 | GPIO40 |
| 34 | IO41 | GPIO41 |
| 35 | IO42 | GPIO42 |
| 36 | RXD0 | U0RXD |
| 37 | TXD0 | U0TXD |
| 38 | IO2 | GPIO2 |
| 39 | IO1 | GPIO1 |
| 40 | GND2 | GND |
| 41 | EPAD | GND |

## 3. USB1 下载串口

| 连接对象 | 原理图网络 | 连接 |
| --- | --- | --- |
| USB1 VBUS | VBUSB | 经 D1 接入 `5V`，同时接 U2-3 `VDD5` 和 U2-9 `VBUS` |
| USB1 D+ | D+ | 接 CH343P U2-7 `UD+` |
| USB1 D- | D- | 接 CH343P U2-8 `UD-` |
| USB1 CC1 | CC1 | R2 5.1 k 到 GND |
| USB1 CC2 | CC2 | R3 5.1 k 到 GND |
| CH343P VIO | VDD33 | U2-1 `VIO` 接 `VDD33`，C10 1 uF 到 GND |
| CH343P V3 | V3 | U2-6 `V3`，C2 100 nF 到 GND |
| CH343P TXD | U0RXD | U2-4 `TXD` 接 ESP32-S3 U4-36 `RXD0` |
| CH343P RXD | U0TXD | U2-5 `RXD` 接 ESP32-S3 U4-37 `TXD0` |
| CH343P DTRTNOW | DTR | U2-12 接自动下载电路 |
| CH343P RTS | RTS | U2-13 接自动下载电路 |
| TX 指示灯 | U0TXD | R1 680 ohm 到 `VDD33`，LED 标注 TX1 |
| RX 指示灯 | U0RXD | R4 680 ohm 到 `VDD33`，LED 标注 RX1 |

## 4. USB2 原生 USB/OTG

| 连接对象 | 原理图网络 | 连接 |
| --- | --- | --- |
| USB2 VBUS | 5V | 经 D2 接入 `5V` |
| USB2 D+ | GPIO20 | 接 ESP32-S3 U4-14 `IO20` |
| USB2 D- | GPIO19 | 接 ESP32-S3 U4-13 `IO19` |
| USB2 CC1 | CC1 | R9 5.1 k 到 GND |
| USB2 CC2 | CC2 | R10 5.1 k 到 GND |
| USB-OTG 焊接位 | USB-OTG | 0 ohm/0603，位于 USB2 VBUS/OTG 相关路径 |

注意：`GPIO19/GPIO20` 是 ESP32-S3 原生 USB D-/D+ 引脚，同时也引出到 J2。作为普通 GPIO 使用会影响 USB2 功能。

## 5. 自动下载、BOOT 与 RST

| 模块/器件 | 连接 | 说明 |
| --- | --- | --- |
| EN 上拉/复位 RC | `CHIP_PU` 经 R8 10 k 上拉到 `VDD33`，C7 1 uF 到 GND | 模组使能脚上电复位 |
| Q1 SS8050 | 由 `DTR`、`RTS` 组合控制 `CHIP_PU` | 自动复位 |
| Q2 SS8050 | 由 `DTR`、`RTS` 组合控制 `GPIO0` | 自动进入下载模式 |
| BOOT 按键 | 一端 GND，一端 `GPIO0` | 手动下载模式按键 |
| BOOT 滤波电容 | C8 | `GPIO0` 到 GND，0.1 uF/0402 | 按键滤波 |
| RST 按键 | 一端 GND，一端 `CHIP_PU` | 手动复位按键 |
| RST 滤波电容 | C9 | `CHIP_PU` 到 GND，0.1 uF/0402 | 按键滤波 |

原理图标注自动下载逻辑为 `DTR RTS --> EN IO0 IO2`。该页可见自动控制对象为 `CHIP_PU` 和 `GPIO0`。

## 6. 板载 RGB LED

| 器件 | 管脚 | 连接 |
| --- | --- | --- |
| U1 XL-5050RGBC-WS2812B | VDD | `VDD33` |
| U1 XL-5050RGBC-WS2812B | GND | GND |
| U1 XL-5050RGBC-WS2812B | DI | 经 RGB 0 ohm/0603 电阻位接 `GPIO48`，网络名 `RGB_CTRL` |
| U1 XL-5050RGBC-WS2812B | DO | 未继续级联 |

注意：新版原理图中板载 WS2812 的控制脚是 `GPIO48`，供电是 `VDD33`。

## 7. J1 排针

| J1 脚号 | 网络 |
| --- | --- |
| 1 | VDD33 |
| 2 | VDD33 |
| 3 | CHIP_PU |
| 4 | GPIO4 |
| 5 | GPIO5 |
| 6 | GPIO6 |
| 7 | GPIO7 |
| 8 | GPIO15 |
| 9 | GPIO16 |
| 10 | GPIO17 |
| 11 | GPIO18 |
| 12 | GPIO8 |
| 13 | GPIO3 |
| 14 | GPIO46 |
| 15 | GPIO9 |
| 16 | GPIO10 |
| 17 | GPIO11 |
| 18 | GPIO12 |
| 19 | GPIO13 |
| 20 | GPIO14 |
| 21 | 5V-INOUT 节点，经 D3/IN-OUT 焊接位到 `5V` |
| 22 | GND |

## 8. J2 排针

| J2 脚号 | 网络 |
| --- | --- |
| 1 | GND |
| 2 | U0TXD |
| 3 | U0RXD |
| 4 | GPIO1 |
| 5 | GPIO2 |
| 6 | GPIO42 |
| 7 | GPIO41 |
| 8 | GPIO40 |
| 9 | GPIO39 |
| 10 | GPIO38 |
| 11 | GPIO37 |
| 12 | GPIO36 |
| 13 | GPIO35 |
| 14 | GPIO0 |
| 15 | GPIO45 |
| 16 | GPIO48 |
| 17 | GPIO47 |
| 18 | GPIO21 |
| 19 | GPIO20 |
| 20 | GPIO19 |
| 21 | GND |
| 22 | GND |

## 9. 使用注意

- `U0TXD/U0RXD` 已连接 CH343P 下载串口，同时也引出到 J2-2/J2-3。
- `GPIO0` 已连接 BOOT 按键和自动下载电路，同时引出到 J2-14。
- `CHIP_PU` 已连接复位按键、自动下载电路、RC 上电复位，同时引出到 J1-3。
- `GPIO19/GPIO20` 是 USB2 原生 USB 数据线，同时引出到 J2-20/J2-19。
- `GPIO48` 连接板载 WS2812 RGB LED，同时引出到 J2-16。
- `GPIO45/GPIO46` 属于 ESP32-S3 启动相关管脚，外接电路使用时需要避免影响启动电平。
