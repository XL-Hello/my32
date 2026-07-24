# LVGL 适配与优化 说明

本文面向嵌入式前端的 LVGL 显示与输入适配，说明本项目的绘图缓冲机制、FrameBuffer 边界、脏矩形刷新、触摸输入、SPI 带宽及优化方向。本文描述的是当前代码实际行为，不替代 ILI9341 数据手册与实物时序验证。

## 1. 当前适配概览

当前实现没有在 MCU 内存中保存完整的屏幕帧。LVGL 使用一个静态绘图缓冲区，先绘制发生变化的区域，再经 SPI DMA 提交到 ILI9341 控制器内部的 GRAM。

```text
LVGL 绘制
  -> buf_1（容量 4800 个 RGB565 像素）
  -> disp_flush()
  -> lcd_draw_bitmap()
  -> esp_lcd_panel_draw_bitmap()
  -> SPI DMA
  -> ILI9341 GRAM
  -> DMA 完成回调
  -> lv_disp_flush_ready()
```

相关实现：

- `lv_port_disp.c`：创建 LVGL 绘图缓冲、注册显示驱动，并在 `disp_flush()` 中提交脏矩形。
- `components/LCD/lcd.c`：初始化 SPI/DMA 与 ILI9341，并将数据传给 `esp_lcd`。
- `components/LCD/include/lcd.h`：定义屏幕物理分辨率。

DMA 完成前，`lcd_flush_ready_callback()` 不会调用 `lv_disp_flush_ready()`；因此 LVGL 不会复用仍在 DMA 传输中的单缓冲区。这个“完成通知”是异步 DMA 场景最重要的适配约束：过早调用会导致 DMA 读到被下一次绘制覆盖的数据，遗漏调用则会使 LVGL 刷新停滞。

## 2. 显示系统基础

### 2.1 绘图缓冲、FrameBuffer 与 LCD GRAM

三个术语容易混淆，但当前项目中它们处于不同位置、承担不同职责：

| 名称 | 所在位置 | 用途 | 本项目状态 |
| --- | --- | --- | --- |
| LVGL 绘图缓冲（draw buffer） | ESP32-S3 内存 | LVGL 生成待发送的像素块 | 已启用，容量 4800 像素 |
| MCU FrameBuffer | ESP32-S3 内存/PSRAM | 保存完整一帧并常由 RGB/LTDC 类硬件持续扫描 | 未使用 |
| ILI9341 GRAM | LCD 控制器内部 | 保存最终显示的整屏像素 | 已使用，SPI 刷新目标 |

ILI9341 是 SPI 接口控制器，显示扫描由其内部 GRAM 完成；ESP32-S3 不需要像 RGB 并行屏那样持续向屏幕提供像素。因此，当前的 `buf_1` 应称为 **LVGL draw buffer**，不应误称为全屏 FrameBuffer。

### 2.2 当前配置与容量计算

| 项目 | 配置/计算 | 实际值 | 单位 |
| --- | --- | ---: | --- |
| 屏幕分辨率 | `LCD_H_RES × LCD_V_RES` | `240 × 320` | 像素 |
| LVGL 颜色格式 | `LV_COLOR_DEPTH=16` | RGB565 | 2 B/像素 |
| 字节序 | `LV_COLOR_16_SWAP=1` | 按 SPI 所需高低字节顺序输出 | - |
| 单绘图缓冲容量 | `MY_DISP_HOR_RES * 20` | `240 × 20 = 4800` | 像素 |
| 单绘图缓冲内存 | `4800 × sizeof(lv_color_t)` | `9600 B`，约 9.4 KiB | 字节 |
| SPI 最大单次传输 | `LCD_V_RES * LCD_TEST_LINES * 2` | `320 × 80 × 2 = 51200 B`，约 50 KiB | 字节 |
| 全屏图像数据 | `240 × 320 × 2` | `153600 B`，即 150 KiB | 字节 |

`lv_disp_draw_buf_init()` 的最后一个参数是**像素数量，不是字节数**。因此当前配置：

```c
static lv_color_t buf_1[MY_DISP_HOR_RES * 20];
lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_HOR_RES * 20);
```

数组容量和 LVGL 声明的容量均为 4800 像素，在 RGB565 下恰好占用 9600 B。

### 2.3 分辨率与显示方向

当前显示端口固定用 `LCD_H_RES=240`、`LCD_V_RES=320` 注册 LVGL，并在初始化中选择 `LCD_ORIENTATION_PORTRAIT_INVERTED`；该方向不改变逻辑尺寸。

若改为横屏，LCD 驱动的逻辑尺寸将变为 `320 × 240`。此时必须同时更新 LVGL 的 `hor_res`/`ver_res`、缓冲区宽度计算和触摸坐标映射；不能只调整其中一项。

## 3. 显示刷新链路

### 3.1 脏矩形、绘图缓冲与 `flush_cb`

一次 UI 变化可使多个对象失效。LVGL 先将它们记录为脏矩形列表 `[A, B, C, ...]`，再在实际刷新前尝试合并。合并仅由几何关系决定：两个矩形必须相交或接触，并且合并后的包围矩形面积必须**小于**原面积之和。相距较远的区域不会为了减少一次 `flush_cb` 调用而合并，否则会重绘大量未变化的像素。

```text
[A, B, C]
  -> 仅合并有面积收益的相邻/相交区域
[AB, C]
  -> 分别绘制并刷新 AB、C
```

脏矩形的合并与 draw buffer 容量没有关系。一个合并后的脏矩形即使大于 4800 像素，仍会保留为一个逻辑脏区；LVGL 在绘制时才根据缓冲区容量将它纵向切成多个连续子块。

当前 `buf_1` 的 `240 * 20` 表示**4800 像素的容量**，不是“每次只能绘制 240 列、20 行”的固定形状。对于宽为 `w`、高为 `h` 的最终脏矩形，LVGL 在未使用 `rounder_cb` 时每块最多可绘制的行数为：

```text
max_rows = min(4800 / w, h)
```

例如，`50 × 50` 脏矩形共 2500 像素，小于缓冲容量；LVGL 将它一次绘制到缓冲区的前 2500 个像素位置，并只发送这个 `50 × 50` 区域，即 5000 B RGB565 数据。缓冲区剩余空间不会绘制或传输。对于 `100 × 100` 脏矩形，最多容纳 `4800 / 100 = 48` 行，将依次形成 `100 × 48`、`100 × 48`、`100 × 4` 三个渲染/传输子块。

这些子块不是新的、可再次合并的脏矩形；它们只是同一个逻辑脏矩形为适配 draw buffer 而产生的连续分片。多个相距很远的独立脏矩形则各自按相同规则分片。

LVGL 8 的 `flush_cb` 每次调用只接收一个连续矩形及其连续像素数据：

```c
flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
```

它不能在一次调用中携带 `[A, B, C]` 这类离散矩形列表。因此三个独立脏矩形会触发多次调用，例如 `flush(A, pixels_a)`、`flush(B, pixels_b)`、`flush(C, pixels_c)`；一个大脏矩形的三个分片同样会触发三次调用。本项目的 `disp_flush()` 将每个 `area` 直接映射为一次 `lcd_draw_bitmap()` 提交。

还应注意，脏矩形限定的是最终像素的更新范围，并不意味着只调用发生变化对象的绘制代码。LVGL 会绘制所有与当前裁剪区域相交、且会影响最终外观的背景、父对象、兄弟对象、透明效果、圆角或阴影；所有操作均被裁剪在该脏矩形内，最终只发送该区域的像素。

### 3.2 提交与 DMA 完成通知

LVGL 的显示尺寸注册为 `240 × 320`，但每次 `flush_cb` 收到的是一个脏矩形或其分片，不必是完整屏幕。若脏区域超过 draw buffer 容量，LVGL 会按该脏区的实际宽度计算可容纳行数并拆成多次刷新；每次传输完成后，才开始下一块。

`trans_queue_depth = 10` 是底层 `esp_lcd` SPI 事务队列深度，不等价于 LVGL 双缓冲，也不会让 LVGL 在当前单缓冲上与 DMA 并行绘制。

当前配置满足：

```text
LVGL 单缓冲传输量：240 × 20 × 2 = 9600 B
SPI max_transfer_sz：51200 B
```

即单个 LVGL 缓冲区传输量小于 SPI DMA 上限。

### 3.3 刷新粒度与调度关系

`lvgl_task()` 每约 33 ms 调用一次 `lv_timer_handler()`，这只是 LVGL 处理定时器和启动重绘的频率上限，**不保证实际显示帧率为 30 FPS**。实际刷新率取决于以下较慢环节：

```text
应用状态变化
  -> LVGL 标记脏区
  -> lv_timer_handler() 发起重绘
  -> CPU 在 draw buffer 渲染
  -> SPI DMA 发送脏区
  -> lv_disp_flush_ready() 允许下一块/下一轮刷新
```

小缓冲不要求脏区高度不超过 20 行。对于更大的脏区，LVGL 会按 draw buffer 容量分块渲染、分块调用 `flush_cb`；分块高度由脏区实际宽度决定。因此缓冲容量主要影响“每次提交的块大小”和 CPU/DMA 可否重叠，不改变一整屏 RGB565 所需发送的总字节数。

在当前单缓冲配置下，DMA 发送期间不能继续使用同一块 `buf_1` 渲染下一块，绘制与发送大多串行；这是以较低 RAM 占用换取实现简单性的选择。

## 4. LVGL 输入设备接入

本项目的触摸输入链路如下。其他输入设备（按键、编码器、鼠标等）也应遵循“硬件采集、设备适配、LVGL 输入适配、UI 事件处理”的分层方式。

```text
HR2046 触摸控制器
  -> GPIO 判断按下状态 + SPI 读取原始 X/Y
  -> 滤波与坐标校准：touch_read_point()
  -> LVGL pointer read_cb
  -> LVGL 输入处理：点击、长按、滚动、手势
  -> LV_EVENT_* 回调
  -> UI / 业务动作
```

### 4.1 当前项目的职责划分

| 层次 | 当前实现 | 职责 |
| --- | --- | --- |
| 硬件层 | `components/LCD/touch.c` | 初始化 GPIO/SPI，读取 HR2046 原始坐标 |
| 设备适配层 | `touch_read_point()` | 原始数据滤波、校准并转换为屏幕坐标 |
| LVGL 输入适配层 | `lv_port_indev.c` | 将触摸点注册为 `LV_INDEV_TYPE_POINTER` |
| UI 事件层 | `home_ui.c` | 处理 `LV_EVENT_GESTURE` 并输出方向日志 |

### 4.2 初始化顺序与输入设备生命周期

初始化顺序必须保证显示、物理触摸和 LVGL 输入设备均可用：

```c
lv_init();
lv_port_disp_init();
touch_init();
lv_port_indev_init();
```

输入驱动描述符必须是静态或其他长期有效对象，因为 LVGL 注册后会一直保存它的指针：

```c
static lv_indev_drv_t s_touch_indev_drv;
```

### 4.3 LVGL 8 pointer 数据格式

当前项目使用 LVGL 8。触摸屏注册为 `LV_INDEV_TYPE_POINTER`，读取回调的签名为：

```c
static void read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
```

每次回调至少应写入一个坐标和状态，并在没有更多样本时清除 `continue_reading`：

```c
data->point.x = x;                          /* 与 LVGL 显示方向一致的逻辑坐标 */
data->point.y = y;
data->state = LV_INDEV_STATE_PRESSED;       /* 或 LV_INDEV_STATE_RELEASED */
data->continue_reading = false;
```

本项目在触摸适配层使用的数据为：

```c
typedef struct {
    uint16_t x;      /* 0..239 */
    uint16_t y;      /* 0..319 */
    bool pressed;    /* 物理按下状态 */
    bool valid;      /* 本次坐标采样是否可信 */
} touch_point_t;
```

转换规则如下：

| 触摸数据 | 交给 LVGL 的状态 |
| --- | --- |
| `pressed && valid` | 坐标 + `LV_INDEV_STATE_PRESSED` |
| 未按下 | `LV_INDEV_STATE_RELEASED` |
| 按下但坐标无效或通信失败 | `LV_INDEV_STATE_RELEASED`，避免状态停留在按下 |

`x/y` 必须使用与 LVGL 显示驱动相同的逻辑方向和分辨率；切换 LCD 方向时，应同时更新触摸校准矩阵。`read_cb` 由 `lv_timer_handler()` 按输入读取周期调用，当前默认约为 30 ms；无需为常规触摸读取额外创建任务。

### 4.4 手势、滚动与事件

LVGL 根据连续的 `PRESSED -> 坐标变化 -> RELEASED` 自动识别点击、长按、滚动与手势。应用可在目标对象或页面上注册 `LV_EVENT_GESTURE`：

```c
static void gesture_cb(lv_event_t *event)
{
    lv_indev_t *indev = lv_event_get_indev(event);
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
}
```

方向为 `LV_DIR_LEFT`、`LV_DIR_RIGHT`、`LV_DIR_TOP` 或 `LV_DIR_BOTTOM`。当前 LVGL 默认要求累计移动超过 50 像素，且单次移动速度达到至少 3 像素/采样，才会产生手势事件。

可滚动对象会优先处理拖动，可能不会派发手势事件。本项目首页为验证滑动手势，已清除 `LV_OBJ_FLAG_SCROLLABLE`；后续同时支持列表滚动与页面翻页时，应明确划分手势区域或处理优先级。

### 4.5 输入适配注意事项

- `read_cb` 应短小且执行时间可预测；避免在其中输出大量日志、执行复杂计算或等待外部事件。
- ISR 中不要调用 LVGL API。若使用中断，应仅记录状态、写入队列或通知采样任务；`read_cb` 仍只读取最终缓存。
- 当前触摸与 LCD 共用 SPI 总线，触摸读取会与显示 DMA 竞争。若显示繁忙时触摸延迟明显，可改为独立采样任务保存最新坐标，让 `read_cb` 仅复制缓存。
- 对电阻触摸屏，滤波、按下去抖和坐标校准通常比缩短读取周期更重要。
- 记录点击、移动和手势的时间戳与坐标，可用于区分采样、LVGL 调度和 UI 刷新各阶段的延迟。

## 5. 性能分析与测量

### 5.1 SPI 接口理论速率

当前 `LCD_PIXEL_CLOCK_HZ` 为 `2 MHz`，SPI 单线 MOSI 在理想情况下每个时钟发送 1 bit：

```text
原始比特率 = 2,000,000 bit/s
原始字节率 = 2,000,000 / 8 = 250,000 B/s，约 244 KiB/s
RGB565 每像素 = 16 bit = 2 B
理论像素率 = 2,000,000 / 16 = 125,000 像素/s
```

按纯像素载荷计算，当前屏幕的理论传输下限如下；实际值会因 CASET/RASET/RAMWR 命令、DMA 启动、任务调度和屏幕控制器时序而更低。

| 刷新区域 | 像素/数据量 | 2 MHz 下理论最短时间 | 仅由链路决定的最高频率 |
| --- | ---: | ---: | ---: |
| 20 行满宽块 | `240 × 20` / 9600 B | 38.4 ms | 26.0 块/s |
| 半屏 | `240 × 160` / 76800 B | 307.2 ms | 3.25 次/s |
| 全屏 | `240 × 320` / 153600 B | 614.4 ms | 1.63 FPS |

因此，`UI_FRAME_PERIOD_MS=33` 只能让 LVGL 最多约每秒检查 30 次更新；当刷新接近全屏时，链路上限约为 1.63 FPS，调小任务周期不会提高显示速度。局部更新则按脏区面积近似线性受益，例如仅更新数字、进度条或小图标时，SPI 数据量远低于全屏。

若将 SPI 时钟提升到 `F`，全屏理论 FPS 可按下式估算：

```text
FPS_theoretical = F / (LCD_H_RES × LCD_V_RES × 16)
```

例如，40 MHz 下全屏载荷理论上限约为 `40,000,000 / (240 × 320 × 16) = 32.55 FPS`；这是忽略命令、软件开销和屏幕模块稳定性的上限，不是实测承诺。

### 5.2 建议测量的指标

现有 `monitor_cb` 以一次 LVGL 脏区刷新周期为单位统计刷新次数；它适合观察 UI 刷新频率，但不等同于单个 DMA 传输块数量，也不严格等同于最后一块 DMA 已完成的屏幕更新频率。性能分析时应区分以下指标：

| 指标 | 起止范围 | 用途 |
| --- | --- | --- |
| 脏区像素数 | 一次 LVGL 刷新周期的最终脏区面积 | 判断 UI 是否产生不必要的大面积重绘 |
| LVGL 刷新频率 | `monitor_cb` 的周期计数 | 观察实际发生 UI 重绘的频率 |
| CPU 渲染时间 | 对象失效至 draw buffer 像素生成 | 定位 CPU 绘制瓶颈 |
| DMA 传输时间 | `lcd_draw_bitmap()` 提交至完成回调 | 测量 SPI/LCD 链路吞吐 |
| 端到端延迟 | UI 状态变化至最后一块 DMA 完成 | 衡量用户可见更新延迟 |

测试应分别覆盖小局部脏区、文字/透明叠加和接近全屏的高负载场景；每个场景预热后记录平均值、P95 与最大值，且测试循环中避免输出高频日志。

## 6. 优化策略与方案选择

### 6.1 缓冲策略选择

#### 单小缓冲（当前方案）

- 内存约 9.4 KiB。
- DMA 传输期间等待同一缓冲区可复用，结构简单、内存占用低。
- 适合当前无 PSRAM 的 ESP32-S3 配置。

#### 双小缓冲

可配置两个较小缓冲区，例如每个 10 行：

```text
每块：240 × 10 × 2 = 4800 B
总计：9600 B
```

DMA 发送一块时，LVGL 可在另一块绘制下一块区域，从而提升绘制和传输的重叠度。两个缓冲区的单次传输量仍低于当前 51200 B 的 DMA 上限。

#### 双全屏 FrameBuffer

两个 RGB565 全屏缓冲区需要：

```text
240 × 320 × 2 × 2 = 307200 B（约 300 KiB）
```

该模式需要开启 `disp_drv.full_refresh = 1`，并要求 SPI 的 `max_transfer_sz` 至少可容纳一整帧，即不小于 153600 B。当前 51200 B 的设置不能直接支持全屏单次传输；同时项目未启用 PSRAM，300 KiB 的静态 DMA 可访问内存会显著增加内存压力，因此不建议直接切换。

更重要的是，SPI 控制器不具备 RGB 屏的“切换扫描地址”能力：即使 LVGL 使用双全屏缓冲，仍需把整帧经 SPI 写入 ILI9341 GRAM。该方案可能减少 LVGL 分块管理，却不能消除 SPI 的全帧带宽成本；对于当前 2 MHz SPI 并不划算。

若以后启用此方案，绘图缓冲描述符的像素数应写为：

```c
MY_DISP_HOR_RES * MY_DISP_VER_RES
```

而不是注释示例中的 `MY_DISP_VER_RES * LV_VER_RES_MAX`。

### 6.2 分项优化建议

| 项目 | 当前情况 | 建议 | 验证标准 |
| --- | --- | --- | --- |
| SPI 时钟 | `2 MHz`，全屏理论仅 1.63 FPS | 从较低的更高频率逐级提高 `LCD_PIXEL_CLOCK_HZ`；每档运行颜色块、文字和连续局部刷新测试 | 无花屏、颜色错误、丢帧或 SPI 超时；记录可稳定运行的最高频率 |
| 脏区控制 | LVGL 自动合并/拆分脏区 | UI 更新时只改动实际变化的对象；避免定时对全屏容器调用无意义的重绘/样式重置 | 使用 `monitor_cb` 的 `pixel_count` 与刷新 FPS 观察数据量是否下降 |
| 绘图缓冲 | 单块 20 行，约 9.4 KiB | RAM 允许时改为两个小缓冲，优先保持每块为整行的整数倍 | 比较相同动画下 `monitor_cb` 时间和可见流畅度，确认无 DMA 数据损坏 |
| 缓冲行数 | 20 行 | 不必盲目增大；在高 SPI 时钟或 CPU 绘制较重时，测试 20、40、80 行的吞吐和内存占用 | 单块字节数始终不超过 `max_transfer_sz`，且系统剩余堆足够 |
| DMA 限制 | `max_transfer_sz=51200 B` | 保持它不小于最大实际 `flush_cb` 数据块；若增大单缓冲或改用全屏缓冲，按最大像素块重新计算 | 无 `ESP_ERR_INVALID_SIZE`、DMA 失败或传输异常 |
| 队列深度 | `trans_queue_depth=10` | 当前单缓冲无需仅为性能盲目增大；双缓冲/驱动分段策略改变后再结合内存测量调整 | 不出现队列满、提交失败，且增大后有实测收益 |
| 全屏 FrameBuffer | 未启用、无 PSRAM | 对 SPI ILI9341 保持小缓冲优先；除非确有复杂合成需求且内存与带宽已验证，否则不要改为双全屏 | 静态 RAM、DMA 可访问性、全屏传输长度和实测帧率均满足需求 |
| LVGL 任务周期 | 33 ms | 对交互 UI 可保留；不要用缩短周期弥补 SPI 带宽不足。仅在需降低输入/动画延迟且 CPU 余量充足时调整 | CPU 占用、触摸延迟、刷新率均有测量结果 |

推荐的实施顺序是：先用 `monitor_cb` 记录实际脏区像素数和刷新率；再逐级提高 SPI 时钟并进行实物稳定性测试；最后才评估双小缓冲。每次只改变一个参数，便于确定瓶颈来源。

## 7. 适配检查清单

- [ ] `disp_drv.hor_res`、`disp_drv.ver_res` 与 LCD 当前方向一致。
- [ ] `lv_disp_draw_buf_init()` 的 size 以像素数填写，数组实际容量不小于该值。
- [ ] `sizeof(lv_color_t)`、LCD `bits_per_pixel` 和发送数据格式一致；SPI RGB565 场景保持正确的字节序设置。
- [ ] 最大一次 `flush_cb` 数据量不超过 SPI `max_transfer_sz`。
- [ ] 使用异步 DMA 时，仅在传输完成回调中调用 `lv_disp_flush_ready()`。
- [ ] 调整方向时，同步验证触摸坐标、控件可视区域及边界刷新。
- [ ] 调高 SPI 时钟或改动缓冲策略后，完成连续刷新、颜色、文本与触摸压力测试。
