# 在 ESP32-S3 上使用 LVGL 驱动 ILI9341 LCD

本文面向第一次接触 LVGL 的开发者，目标是回答一个实际问题：

> 已经有一块带 ILI9341、HR2046 触摸和 SD 卡槽的 2.8 寸 LCD，怎样从“接线”一步步做到“屏幕显示 LVGL UI，并能响应触摸”？

本文针对当前仓库的模块：

- LCD 控制器：`ILI9341`
- 分辨率：原生竖屏 `240 × 320`，横屏可配置为 `320 × 240`
- 显示接口：SPI
- 像素格式：16-bit `RGB565`
- 触摸控制器：丝印 `HR20462045`，按 HR2046/XPT2046 兼容系列理解
- 目标芯片：`ESP32-S3`

相关硬件引脚、显示协议和 HR2046 信息见：[2.8 寸 ILI9341 TFT LCD 模块](./lcd_2.8_tft_240*rgb*320.md)。

---

## 1. 先建立整体认识

LVGL 不是 LCD 驱动，也不会直接操作 ESP32 的 GPIO。它是一个运行在单片机上的图形界面库，负责：

- 创建按钮、标签、进度条、页面等 UI 对象；
- 计算控件布局和样式；
- 判断哪些区域发生了变化；
- 生成需要显示的像素数据；
- 处理点击、拖动等输入事件。

LVGL 不知道你的 LCD 使用哪种 SPI 芯片，也不知道触摸屏接在哪些 GPIO。我们需要在 LVGL 和硬件之间编写“适配层”。

完整显示链路：

```text
LVGL UI 对象
    ↓ 计算布局、绘制控件
LVGL 绘图缓冲区（RGB565 像素）
    ↓ flush 回调
esp_lcd 面板驱动
    ↓ 设置窗口、发送 0x2C 和像素数据
SPI 总线
    ↓
ILI9341 LCD 显存
    ↓
屏幕显示图像
```

触摸是反向链路：

```text
手指按压
    ↓
四线电阻屏
    ↓
HR2046 采样 X/Y 原始值
    ↓ SPI 读取、滤波、校准、旋转映射
LVGL 输入设备 read_cb
    ↓
按钮收到 pressed/released 事件
```

要显示一个按钮，至少要打通四个部分：

1. LCD 能被 ESP32 初始化；
2. LVGL 能把像素交给 LCD；
3. LVGL 的时间和任务调度正常；
4. UI 对象已经创建并加载到屏幕。

触摸属于第五个部分，可以在屏幕先显示 UI 后再接入。

## 2. 当前仓库的真实状态

目前仓库中的 `app/main.c` 已经初始化 ILI9341，并启动纯色测试任务；`components/LCD` 已包含 SPI2、`esp_lcd` 和 ILI9341 panel 驱动。应用组件尚未引入 LVGL，因此当前还不能显示 LVGL UI。

| 能力 | 当前状态 |
| --- | --- |
| ESP32-S3 工程 | 已存在 |
| ILI9341 接线资料 | 已整理 |
| HR2046 资料 | 已整理，具体协议仍需实测确认 |
| SPI LCD 驱动 | 已加入，纯色显示已实机验证 |
| LVGL 依赖 | 尚未加入当前应用 |
| LVGL 显示适配层 | 尚未加入 |
| LVGL 触摸适配层 | 尚未加入 |
| LCD GPIO 分配 | 已确定，`CS` 使用 GPIO7 |
| 可显示 UI 的示例代码 | 尚未加入 |

所以，“从 LCD 到 LVGL”不是只添加一个 `lv_label_create()`，而是要完成一条硬件和软件链路。

### 当前调试记录

截至目前，已完成 LCD 背光和 ILI9341 显示链路验证：

```text
LCD VCC → 3.3V
LCD GND → ESP32-S3 GND
LCD LED → 3.3V
验证结果：屏幕背光已点亮
```

背光验证只说明电源和背光回路正常；现在的纯色实测则进一步证明 `SCK`、`MOSI`、`CS`、`DC`、`RESET`、ILI9341 初始化和 RGB565 像素发送链路可以工作。

软件和实机调试进度：`components/LCD` 已实现 SPI2、ILI9341 初始化、RGB565 分块填充及红、绿、蓝、白、黑五色循环；`app/main.c` 调用 `lcd_init()` 与 `lcd_test_colors()` 后，屏幕已经能显示不同纯色。实际 `CS` 已从此前记录的 GPIO10 改为 **GPIO7**，对应 `components/LCD/include/lcd.h` 中的 `LCD_PIN_CS`。

下一阶段不再继续增加纯色图案，而是依次完成：创建 LVGL 单绘图缓冲区、注册显示驱动、启动 LVGL tick 与任务、创建第一个 UI demo。开始前应停止 `lcd_test_colors()`：它与 LVGL 都会向同一个 ILI9341 panel 发起绘制，不能同时运行。

## 3. LVGL 适配所涉及的角色

### 3.1 ILI9341 驱动

ILI9341 负责接收命令和 RGB565 像素数据，并把数据写入内部显存。它不理解“按钮”或“文字”，只知道坐标窗口和像素。

常见的刷屏操作是：

```text
设置列地址 0x2A
设置页地址 0x2B
发送内存写命令 0x2C
连续发送 RGB565 像素
```

### 3.2 `esp_lcd`

`esp_lcd` 是 ESP-IDF 提供的 LCD 抽象层。它把 SPI 总线、`CS`、`DC`、复位和面板控制器封装起来，向上层提供统一的：

```c
esp_lcd_panel_reset(panel_handle);
esp_lcd_panel_init(panel_handle);
esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, color_data);
```

当前仓库内置的 ESP-IDF 示例 `esp-idf/examples/peripherals/lcd/spi_lcd_touch` 展示了 ILI9341、SPI 和 LVGL 的连接方式。

### 3.3 LVGL 显示适配层

LVGL 绘制完成后会调用我们注册的 `flush_cb`。这个回调要做的事情是：

```text
接收 LVGL 要更新的区域 area
接收该区域的 RGB565 缓冲区 color_map
调用 esp_lcd_panel_draw_bitmap()
传输结束后通知 LVGL flush 完成
```

若使用异步 DMA，SPI 传输完成回调中必须调用 `lv_disp_flush_ready()`；否则 LVGL 会一直等待，后续界面不再刷新。

### 3.4 LVGL 输入驱动适配层

HR2046 读取到的是原始 ADC 坐标，LVGL 需要的是：

```text
是否按下
屏幕坐标 x
屏幕坐标 y
```

因此触摸适配层需要完成：SPI 读取、去抖、压力/按下判断、校准、横竖屏坐标变换，然后在 `read_cb` 中交给 LVGL。

### 3.5 LVGL 时间基准和任务

LVGL 需要知道时间已经过去了多少毫秒，用于动画、长按、定时器和输入处理。还需要周期调用：

```c
lv_tick_inc(elapsed_ms);
lv_timer_handler();
```

常见做法是：

- 使用 `esp_timer` 每 1～2 ms 调用 `lv_tick_inc()`；
- 创建一个 LVGL 任务，每 5～10 ms 调用 `lv_timer_handler()`；
- 所有 LVGL 对象的创建和修改都放在同一个 LVGL 任务中，避免并发访问。

## 4. 从硬件到 UI 的完整工作顺序

建议严格按下面的顺序开发。每一步都先看到可观察结果，再进入下一步。

### 第 0 步：确认硬件和 GPIO

先把实际接线填入统一的配置表，不要在多个 C 文件中散落 GPIO 数字。

```c
#define LCD_HOST        SPI2_HOST
#define LCD_PIN_SCLK    GPIO_NUM_xx
#define LCD_PIN_MOSI    GPIO_NUM_xx
#define LCD_PIN_MISO    GPIO_NUM_xx
#define LCD_PIN_CS      GPIO_NUM_xx
#define LCD_PIN_DC      GPIO_NUM_xx
#define LCD_PIN_RST     GPIO_NUM_xx
#define LCD_PIN_BL      GPIO_NUM_xx
#define TOUCH_PIN_CS    GPIO_NUM_xx
#define TOUCH_PIN_IRQ   GPIO_NUM_xx
```

这里的 `GPIO_NUM_xx` 必须替换成真实 GPIO。当前仓库没有为 LCD 指定固定引脚，不能直接照抄其他示例。

硬件上要确认：

- ESP32-S3 与模块共地；
- `VCC` 电压符合模块要求；
- `LCD_CS`、`T_CS`、`SD_CS` 独立；
- `MOSI/MISO/SCK` 可以共享，但未选中的设备必须释放 MISO；
- `LED` 的高低电平有效性已经确认；
- LCD 和触摸 IO 电平不会超过 ESP32-S3 的允许范围。

### 第 1 步：只验证 LCD 背光

先不初始化 SPI，也不使用 LVGL，只控制 `LED`。背光亮不代表 LCD 已经初始化，背光和显示数据是两条独立链路。

当前验证结果：背光已通过直接连接 `3.3V` 点亮。后续如果需要软件控制亮度，再确认模块 `LED` 引脚的有效电平和背光电流，决定使用 GPIO 逻辑控制、PWM 或 MOSFET 驱动；在 ILI9341 显示验证阶段可以保持 `LED` 直接连接 `3.3V`。

### 第 2 步：初始化 SPI 总线

当前实际接线已经确定如下：

| LCD 引脚 | ESP32-S3 |
| --- | --- |
| `VCC` | `3.3V` |
| `GND` | `GND` |
| `SCK` | `GPIO12` |
| `SDI/MOSI` | `GPIO11` |
| `SDO/MISO` | `GPIO13` |
| `CS` | `GPIO7` |
| `DC` | `GPIO9` |
| `RESET` | `GPIO8` |
| `LED` | `3.3V` |

本阶段的目标是：不接入 LVGL，先用 `components/LCD` 初始化 SPI 和 ILI9341，并依次显示红、绿、蓝、白、黑五种纯色。实现代码位于 `components/LCD`，应用启动时由 `app/main.c` 调用 `lcd_init()` 和 `lcd_test_colors()`。

使用 ESP-IDF SPI master 初始化 SCK、MOSI、MISO，并设置合适的 DMA 传输长度。

```c
spi_bus_config_t bus_config = {
    .sclk_io_num = LCD_PIN_SCLK,
    .mosi_io_num = LCD_PIN_MOSI,
    .miso_io_num = LCD_PIN_MISO,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = LCD_H_RES * 20 * sizeof(uint16_t),
};
ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));
```

当前代码使用 `40 MHz`（`LCD_PIXEL_CLOCK_HZ`），且纯色显示已验证通过。若接入 LVGL 后出现花屏、偶发超时或颜色错误，可先降回 `20 MHz` 排除信号完整性问题。SPI 频率越高，刷新越快，但长线、杜邦线、供电和模块电平转换都会降低稳定性。

### 第 3 步：创建 LCD SPI IO 和 ILI9341 panel

`esp_lcd` 通常分两层：

```text
SPI bus
  ↓
panel IO：负责 SPI、CS、DC、事务队列
  ↓
ILI9341 panel：负责复位、初始化、方向和绘图窗口
```

典型配置关系如下：

```c
esp_lcd_panel_io_spi_config_t io_config = {
    .dc_gpio_num = LCD_PIN_DC,
    .cs_gpio_num = LCD_PIN_CS,
    .pclk_hz = 20 * 1000 * 1000,
    .lcd_cmd_bits = 8,
    .lcd_param_bits = 8,
    .spi_mode = 0,
    .trans_queue_depth = 10,
};

esp_lcd_panel_dev_config_t panel_config = {
    .reset_gpio_num = LCD_PIN_RST,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
    .bits_per_pixel = 16,
};
```

随后执行：

```c
esp_lcd_new_panel_io_spi(..., &io_config, &io_handle);
esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle);
esp_lcd_panel_reset(panel_handle);
esp_lcd_panel_init(panel_handle);
esp_lcd_panel_mirror(panel_handle, ...);
esp_lcd_panel_disp_on_off(panel_handle, true);
```

`RGB/BGR`、镜像和交换轴不是固定答案。如果颜色错乱或方向不对，优先检查这些参数。

### 第 4 步：不使用 LVGL，先显示纯色

当前纯色验证代码已经加入 `components/LCD`：

```text
lcd_init()
  ├── spi_bus_initialize(SPI2_HOST)
  ├── esp_lcd_new_panel_io_spi()
  ├── esp_lcd_new_panel_ili9341()
  ├── esp_lcd_panel_reset()
  ├── esp_lcd_panel_init()
  └── esp_lcd_panel_disp_on_off(true)

lcd_test_colors()
  └── 创建循环任务
      └── lcd_fill()：按 20 行一块调用 esp_lcd_panel_draw_bitmap()
          └── 等待 SPI 彩色事务完成后再复用 DMA 缓冲区
```

应用启动后，串口应看到 `ILI9341 initialized` 和 `LCD color test task started`，随后颜色日志会持续循环输出，屏幕每秒切换一次红、绿、蓝、白、黑。如果构建时缺少 `esp_lcd_ili9341`，需要先通过 ESP-IDF Component Manager 获取 `espressif/esp_lcd_ili9341` 依赖。

当前构建结果：ESP-IDF 5.1.2 已成功解析 `espressif/esp_lcd_ili9341`，工程已生成 `build/hello_world.bin`，并已完成实机纯色显示验证。下一步是接入 LVGL；若之后出现问题，先恢复纯色测试确认底层显示链路仍正常。

在接入 LVGL 之前，先调用 `esp_lcd_panel_draw_bitmap()` 显示红、绿、蓝和白色矩形。

建议验证：

1. 全屏红色；
2. 全屏绿色；
3. 全屏蓝色；
4. 四角绘制不同颜色；
5. 绘制一个指定坐标的矩形。

若纯色都不能稳定显示，不要继续排查 LVGL。

### 第 5 步：加入 LVGL 依赖并停止纯色测试

本项目不使用 ESP-IDF Component Manager 的 LVGL 包，而是将 LVGL 源码固定在仓库内的 `components/lvgl`，由 Git 记录版本。这样可以离线构建、审查源码，并且不会因为组件管理器自动升级而改变 API。

当前工作区已下载 **LVGL 8.4.0**，它与本文使用的 `lv_disp_drv_t`、`lv_disp_draw_buf_t` 和 `lv_disp_flush_ready()` 等 LVGL 8 API 兼容，后续统一以此版本移植。不要将 LVGL 9 源码放入此目录；其显示驱动 API 不兼容本章代码。

目前源码位于 `components/LVGL/lvgl-8.4.0/`，这还不是 ESP-IDF 可识别的组件目录：`components/LVGL/` 本身没有 `CMakeLists.txt`，而真正的组件文件又多嵌套了一层。请先在仓库根目录执行下面的整理命令，使 LVGL 根目录直接成为组件目录：

```bash
mv components/LVGL/lvgl-8.4.0 components/lvgl
rmdir components/LVGL
```

整理后的关键结构必须是：

```text
components/
└── lvgl/
    ├── CMakeLists.txt
    ├── lvgl.h
    ├── lv_conf_template.h
    └── src/
```

上面的“手动下载并整理”与 Git submodule 是二选一的方案。当前已有手动下载的 `v8.4.0`，应继续使用它，**不要**再执行下面的 submodule 命令。若以后在一个没有 `components/lvgl` 的全新仓库中希望由 Git 管理上游源码，可改用 Git submodule：

```bash
git submodule add --branch v8.4.0 https://github.com/lvgl/lvgl.git components/lvgl
git submodule update --init --recursive
```

这会创建 `components/lvgl/` 和 `.gitmodules`。以后克隆本项目时，使用：

```bash
git clone --recurse-submodules <本项目仓库地址>
```

如果不希望使用 submodule，也可以保留当前手动下载的 `v8.4.0` 源码；只要整理后目录满足 `components/lvgl/lvgl.h`、`components/lvgl/src/`、`components/lvgl/CMakeLists.txt` 即可。无论采用哪种方式，**不要**把 LVGL 放在 `components/lvgl/lvgl/` 这一层额外目录中，否则 ESP-IDF 无法把它识别为名为 `lvgl` 的组件。

LVGL 根目录自带面向 ESP-IDF 的 `CMakeLists.txt`，检测到 `ESP_PLATFORM` 后会注册自身源码；无需另写一个覆盖 LVGL 源码的 `CMakeLists.txt`。首次导入后，复制配置模板：

```bash
cp components/lvgl/lv_conf_template.h components/lvgl/lv_conf.h
```

编辑 `components/lvgl/lv_conf.h`，将文件顶部的：

```c
#if 0
```

改为：

```c
#if 1
```

对于当前 ILI9341 + SPI + 单缓冲的首个 demo，先确认以下配置：

```c
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_TICK_CUSTOM 0
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_USE_PERF_MONITOR 1
```

`LV_COLOR_16_SWAP` 必须先保持 `0`，因为当前 `esp_lcd_panel_draw_bitmap()` 路径已经能正确显示 RGB565 纯色。只有接入 LVGL 后发现红蓝对调或 16 位像素字节序错误，才结合实际屏幕现象调整该值；不要在没有现象时同时修改 LCD 的 `RGB/BGR` 和 LVGL 的字节交换配置。

随后在 `app/CMakeLists.txt` 的 `REQUIRES` 中加入本地组件名 `lvgl`、`esp_timer` 和 `LCD`。如果后续把 LVGL 适配层放进独立组件，则将这些依赖移到该组件，避免在多个组件中重复维护。

```cmake
idf_component_register(
    SRCS "main.c" "lvgl_port.c" "ui_demo.c"
    INCLUDE_DIRS "."
    REQUIRES LCD lvgl esp_timer
)
```

`app/idf_component.yml` 不需要添加 `lvgl/lvgl` 依赖。修改本地源码或 CMake 后运行：

```bash
idf.py reconfigure
idf.py build
```

然后将 `app/main.c` 中的：

```c
ESP_ERROR_CHECK(lcd_test_colors());
```

替换为后续的 `lvgl_port_init()`。纯色测试函数保留在 `components/LCD`，作为底层回归测试入口，但 LVGL UI 运行期间不调用它。

导入后可先执行下面命令确认本地组件被识别，再开始编写适配层：

```bash
idf.py reconfigure
idf.py build
```

构建日志中应出现 `components/lvgl` 的编译文件；若出现 `lvgl.h: No such file or directory`，优先检查目录是否多嵌套了一层，以及 `app/CMakeLists.txt` 是否包含 `REQUIRES lvgl`。

### 第 6 步：创建 LVGL 绘图缓冲区（先使用单缓冲）

对于 `240 × 320`、RGB565 的屏幕：

```text
整屏缓冲区 = 240 × 320 × 2 = 153600 字节
20 行缓冲区 = 240 × 20 × 2 = 9600 字节
```

ESP32-S3 可以先使用 20 行缓冲区，即 `9600` 字节。DMA 缓冲区必须位于 SPI DMA 可访问的内存区域。当前阶段只分配 `buf1`，第二个缓冲区传 `NULL`：

```c
static lv_color_t *s_draw_buf_1;
static lv_disp_draw_buf_t s_draw_buf;

s_draw_buf_1 = heap_caps_malloc(LCD_H_RES * 20 * sizeof(lv_color_t),
                                 MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
assert(s_draw_buf_1 != NULL);
lv_disp_draw_buf_init(&s_draw_buf, s_draw_buf_1, NULL, LCD_H_RES * 20);
```

单缓冲时，LVGL 在本次 `flush_cb` 被标记完成前不会复用这块缓冲区，因此 SPI 异步传输仍然安全。双缓冲会消耗更多内存，但可以让 LVGL 绘制下一块区域时 SPI 同时发送另一块区域；等 UI 稳定后再优化。

### 第 7 步：注册 LVGL 显示驱动

以下是 LVGL 8 风格的核心关系：

```c
static lv_disp_drv_t s_disp_drv;

lv_disp_drv_init(&s_disp_drv);
s_disp_drv.hor_res = LCD_H_RES;
s_disp_drv.ver_res = LCD_V_RES;
s_disp_drv.flush_cb = lcd_flush_cb;
s_disp_drv.draw_buf = &s_draw_buf;
s_disp_drv.user_data = lcd_get_panel_handle();
lv_disp_drv_register(&s_disp_drv);
```

为避免应用层直接访问 `components/LCD/lcd.c` 的私有 `s_panel`，需要在 `lcd.h` 增加 `lcd_get_panel_handle()`，返回 `esp_lcd_panel_handle_t`；并在 `lcd.c` 中实现它。LCD 组件还应提供一个“设置颜色传输完成通知”的接口，供 LVGL 安装自己的完成回调。

`flush_cb` 负责把 LVGL 指定区域送到 LCD：

```c
static void lcd_flush_cb(
    lv_disp_drv_t *drv,
    const lv_area_t *area,
    lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel = drv->user_data;

    esp_lcd_panel_draw_bitmap(
        panel,
        area->x1,
        area->y1,
        area->x2 + 1,
        area->y2 + 1,
        color_map);

    // 本项目的 panel IO 使用 DMA 异步传输；这里不能立即调用
    // lv_disp_flush_ready()，而是要在 SPI 颜色传输完成回调中通知 LVGL。
}
```

注意：`x2` 和 `y2` 是包含边界，`esp_lcd_panel_draw_bitmap()` 的结束坐标通常是不包含边界，所以需要传 `x2 + 1`、`y2 + 1`。

当前 `lcd.c` 的颜色传输完成回调用于释放纯色测试的信号量。切换至 LVGL 时，将它改为保存当前 `lv_disp_drv_t *`，并在回调中调用：

```c
lv_disp_flush_ready(s_pending_disp_drv);
```

回调运行在 SPI ISR 上下文，不能在其中创建 UI 对象或调用其他复杂 LVGL API；只完成 flush 通知即可。单缓冲方案一次只允许一个尚未完成的 flush，因此在 `lcd_flush_cb()` 中记录的 `s_pending_disp_drv` 不会发生覆盖。

### 第 8 步：启动 LVGL 时间和任务

LVGL 至少需要：

```c
lv_tick_inc(2);       // 时间过去 2 ms
lv_timer_handler();   // 处理动画、输入、重绘和定时器
```

推荐结构：

```text
esp_timer，每 2 ms
    └── lv_tick_inc(2)

LVGL FreeRTOS task，每 5～10 ms
    └── lv_timer_handler()
```

不要在多个任务中同时调用 LVGL API。所有页面切换、控件创建、控件属性修改都应放在 LVGL 任务中，或通过队列/事件通知 LVGL 任务执行。

```c
static void lvgl_tick_callback(void *arg)
{
    (void)arg;
    lv_tick_inc(2);
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

初始化顺序应固定为：

```text
lcd_init()
  → lv_init()
  → 分配单绘图缓冲区
  → 注册 display driver
  → 创建并启动 2 ms esp_timer（调用 lv_tick_inc）
  → 创建 LVGL task（调用 lv_timer_handler）
  → 在 LVGL task 上下文创建 UI demo
```

建议把 UI demo 的创建通过任务通知或队列交给 `lvgl_task()` 执行。初始化函数在创建任务前直接创建 demo 也可行，但运行期间任何其他任务都不能直接调用 `lv_label_set_text()`、`lv_obj_create()` 等 LVGL API。

### 第 9 步：创建第一个 UI demo

确认显示驱动已经注册后，再创建 UI：

```c
static void ui_button_event_cb(lv_event_t *event)
{
    static uint32_t click_count;
    lv_obj_t *label = lv_event_get_user_data(event);

    ++click_count;
    lv_label_set_text_fmt(label, "Button clicked: %" PRIu32, click_count);
}

static void ui_demo_create(void)
{
    lv_obj_t *screen = lv_scr_act();

    lv_obj_set_style_bg_color(screen, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "ESP32-S3 + ILI9341");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 32);

    lv_obj_t *result = lv_label_create(screen);
    lv_label_set_text(result, "LVGL single-buffer ready");
    lv_obj_align(result, LV_ALIGN_CENTER, 0, 24);

    lv_obj_t *button = lv_btn_create(screen);
    lv_obj_set_size(button, 150, 52);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, -32);
    lv_obj_add_event_cb(button, ui_button_event_cb, LV_EVENT_CLICKED, result);

    lv_obj_t *button_text = lv_label_create(button);
    lv_label_set_text(button_text, "UI demo");
    lv_obj_center(button_text);
}
```

触摸尚未接入时，屏幕应至少显示深色背景、标题、状态文本和 `UI demo` 按钮；按钮不会响应点击是正常现象。接入 HR2046 并注册输入设备后，点击按钮会更新计数文本，从而同时验证显示刷新、LVGL 任务和触摸事件。

如果这段运行后屏幕稳定显示文字，说明以下链路已经打通：

```text
LVGL 对象 → LVGL 绘制 → flush_cb → esp_lcd → SPI → ILI9341
```

## 5. 加入 HR2046 触摸输入

触摸可以在 UI 显示成功后再加入，不建议一开始同时调试 LCD、LVGL 和触摸。

### 5.1 触摸初始化

HR2046 与 LCD 通常共享 `SCK/MOSI/MISO`，但必须使用独立的 `T_CS`。读取触摸时：

```text
LCD_CS = 1
SD_CS  = 1
T_CS   = 0
发送 HR2046/XPT2046 兼容命令
读取 raw_x/raw_y
T_CS   = 1
```

常见命令字节是：

```text
X：0xD0
Y：0x90
```

但 `HR20462045` 的完整原厂资料尚未确认，首次使用必须通过日志或逻辑分析仪确认返回数据。

### 5.2 坐标校准和旋转

触摸返回的是原始 ADC 值，必须转换为 LVGL 坐标：

```text
raw_x/raw_y
    ↓ 去抖、去异常值
    ↓ 四点校准
    ↓ swap_xy / mirror_x / mirror_y
LVGL x/y：x∈[0,239]，y∈[0,319]
```

如果使用横屏 `320 × 240`，要同时改变 LVGL 的 `hor_res/ver_res` 和触摸坐标映射。LCD 的 `MADCTL` 方向与触摸坐标方向必须配套，否则会出现“显示横屏但点击位置偏移”。

### 5.3 注册 LVGL 输入设备

LVGL 8 风格的输入回调逻辑如下：

```c
static void touch_read_cb(
    lv_indev_drv_t *drv,
    lv_indev_data_t *data)
{
    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    bool pressed = false;

    hr2046_read_raw(&raw_x, &raw_y, &pressed);

    if (pressed) {
        data->point.x = touch_map_x(raw_x, raw_y);
        data->point.y = touch_map_y(raw_x, raw_y);
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void touch_register(void)
{
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);
}
```

`T_IRQ` 可以减少无触摸时的 SPI 读取；初次调试也可以不接 `T_IRQ`，按固定周期轮询。ISR 中不要直接执行 SPI 和 LVGL 操作，只发送任务通知或记录状态。

## 6. “点亮屏幕显示 UI”最小实施清单

### 硬件

- [ ] 确认 ESP32-S3 的 SPI 主机和 GPIO；
- [ ] 接好 `VCC/GND/SCK/MOSI/MISO/CS/DC/RESET`；
- [ ] 确认背光 `LED` 的有效电平和供电方式；
- [ ] 确认 LCD、HR2046、SD 卡的 `CS` 不重复；
- [ ] 确认模块 IO 电平与 ESP32-S3 匹配。

### 纯 LCD 驱动

- [ ] 添加 `esp_lcd` 和 ILI9341 panel driver；
- [ ] 初始化 SPI bus；
- [ ] 初始化 LCD panel IO；
- [ ] 执行 LCD reset/init/display-on；
- [ ] 显示红、绿、蓝纯色；
- [ ] 确认方向、颜色顺序和坐标窗口。

### LVGL 显示适配

- [ ] 加入 LVGL component；
- [ ] 在 `app/CMakeLists.txt` 添加 `lvgl`、`esp_lcd`、`esp_timer` 等依赖；
- [ ] 分配 DMA 绘图缓冲区；
- [ ] 注册 LVGL display driver；
- [ ] 实现 `flush_cb`；
- [ ] 在 SPI 事务完成后调用 `lv_disp_flush_ready()`；
- [ ] 启动 `lv_tick_inc()`；
- [ ] 周期调用 `lv_timer_handler()`；
- [ ] 创建一个 label 或 button 验证 UI。

### 触摸输入

- [ ] 确认 HR2046 的 SPI 读回数据；
- [ ] 实现 raw X/Y 读取；
- [ ] 添加去抖和按下判断；
- [ ] 完成四点校准；
- [ ] 处理横竖屏、交换轴和镜像；
- [ ] 注册 LVGL pointer input device；
- [ ] 点击按钮验证 pressed/released 事件。

## 7. 推荐的工程文件组织

为了避免把所有代码塞进 `app/main.c`，建议后续按职责拆分：

```text
app/
├── main.c              # app_main，启动各模块
├── lcd_port.c          # SPI、背光、ILI9341、esp_lcd
├── lcd_port.h
├── touch_port.c        # HR2046 SPI 读取、滤波、校准
├── touch_port.h
├── lvgl_port.c         # LVGL tick、任务、显示/输入注册
├── lvgl_port.h
├── ui.c                # 页面和控件创建
├── ui.h
└── CMakeLists.txt      # 声明源文件和组件依赖
```

推荐的启动顺序：

```c
void app_main(void)
{
    board_init();
    lcd_port_init();
    lcd_show_test_pattern();
    lvgl_port_init();
    touch_port_init();
    ui_create();
}
```

实际实现时，`lvgl_port_init()` 应确保 LVGL 的显示驱动、时间基准和任务已经准备好；`ui_create()` 必须在 LVGL 初始化后执行。

## 8. 常见现象和排查顺序

| 现象 | 优先检查 |
| --- | --- |
| 背光亮但全黑 | `RESET`、`DC`、`CS`、SPI 时钟、ILI9341 初始化 |
| 纯色测试也花屏 | SPI 频率、MOSI、供电、DMA 缓冲区、颜色字节序 |
| 颜色偏红/偏蓝 | `RGB/BGR` 设置、RGB565 高低字节顺序 |
| 图像旋转/镜像错误 | `MADCTL`、`swap_xy`、`mirror_x/y` |
| UI 只显示一次后不更新 | 是否调用 `lv_timer_handler()`、是否调用 `lv_disp_flush_ready()` |
| UI 显示但动画不动 | `lv_tick_inc()` 是否周期执行 |
| LVGL 卡死 | 是否有多个任务同时调用 LVGL，flush 完成通知是否缺失 |
| 触摸坐标整体偏移 | 四点校准、显示区域偏移、坐标映射 |
| 触摸 X/Y 反了 | `swap_xy` 或旋转映射 |
| LCD 正常但触摸无数据 | `T_CS`、MISO 共享、HR2046 命令和 SPI mode |
| 加入 SD 卡后触摸失效 | `SD_CS` 是否在触摸读取期间保持无效，MISO 是否正确释放 |

日志建议至少包含：

```text
LCD SPI bus initialized
ILI9341 reset/init completed
LCD test pattern flushed
LVGL display registered
LVGL task started
HR2046 raw: x=..., y=..., pressed=...
```

## 9. 最小成功标准

1. 上电后背光状态正确；
2. 串口打印 LCD 初始化成功；
3. 屏幕能稳定显示纯色；
4. 屏幕能显示 `ESP32-S3 + ILI9341` 文本；
5. LVGL 按钮能显示并重绘；
6. 加入触摸后，串口能看到合理的原始坐标；
7. 校准后点击按钮能看到 pressed/released 事件；
8. LVGL 动画、定时器和页面切换可以正常运行。

做到第 4 步，说明 LCD 到 LVGL 的显示链路基本打通；做到第 7 步，才算完成当前模块的触摸 UI 适配。

## 10. 学习建议

建议按以下顺序学习和提交代码：

```text
GPIO 背光
  → SPI 基础
  → ILI9341 纯色显示
  → esp_lcd panel API
  → LVGL label
  → LVGL button
  → LVGL tick 和 timer
  → HR2046 原始坐标
  → 坐标校准
  → LVGL 触摸事件
  → 页面和业务逻辑
```

每次只增加一个变量。这样出现问题时，可以快速判断故障是在供电、SPI、LCD 驱动、LVGL 刷新、时间调度，还是触摸坐标转换。
