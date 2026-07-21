# 实物触摸接入、坐标校准与 UI 手势实施说明

本文用于在当前工程中接入 2.8 寸 ILI9341 模块上的 HR2046（按 XPT2046 兼容协议验证）电阻式单点触摸。目标是先把实物触点稳定、准确地送入 LVGL 8 pointer 输入设备，再在 UI 层启用页面切换和控制中心手势。

本文是实施说明，不包含可直接合入的驱动代码。所有“待实测”项必须以目标板串口日志和屏幕标记测试结果为准，不可套用其他屏或其他 HR2046 模块的校准常量。

## 1. 现有项目结论

当前 UI 层尚未接入触摸；这不是配置缺失，而是驱动、坐标变换和 LVGL 输入设备三部分都还没有实现。

| 项目 | 现状 | 对实施的影响 |
| --- | --- | --- |
| LCD | `components/LCD/lcd.c` 使用 ILI9341 和 `SPI2_HOST` | 触摸应作为同一 SPI 总线上的第二个设备，不能再次初始化该总线。 |
| LCD SPI 引脚 | SCLK=GPIO6、MOSI=GPIO7、MISO=GPIO5、LCD_CS=GPIO17、DC=GPIO15、RST=GPIO16 | 触摸的 `T_CLK/T_IN/T_DO` 应分别复用 GPIO6/7/5；`T_CS` 固定使用 GPIO8。 |
| 显示尺寸 | `LCD_H_RES=240`、`LCD_V_RES=320` | LVGL 的逻辑输入坐标范围必须为 `x=0..239`、`y=0..319`。 |
| 当前显示方向 | `lv_port_disp.c` 在 LCD 初始化后设为 `LCD_ORIENTATION_PORTRAIT_INVERTED`；其实现为 `swap_xy=false`、`mirror_x=true`、`mirror_y=false` | 这是触摸方向校准的唯一显示基准。不能按普通竖屏或资料中的横屏公式直接映射。 |
| LVGL | 工程使用 LVGL 8 风格的 `lv_disp_drv_t`；输入读取周期为 30 ms | 应使用 `lv_indev_drv_t`、`LV_INDEV_TYPE_POINTER` 和 `lv_indev_drv_register()`；一次读点应足够快，不能阻塞 30 ms。 |
| LVGL 调用上下文 | `lvgl_task()` 独占调用 `lv_timer_handler()` 并创建 UI | 触摸读取回调和 UI 手势处理都在该任务上下文运行；其他任务、GPIO ISR 不得直接调用 LVGL API。 |
| 触摸驱动 | 未找到 HR2046/XPT2046 驱动、触摸 GPIO、raw 坐标日志或输入设备注册 | 下文第 3～6 步均为必做项。 |
| WS2812B | 当前 CMake 默认使用 RMT，因而不占 SPI2；若改为 SPI 后端会使用 `SPI2_HOST` | 在触摸接入前保持 RMT。若必须用 WS2812B SPI 后端，应先重新规划总线所有权和引脚。 |

模块资料中 `HR20462045` 只能暂按 HR2046/XPT2046 兼容系列处理：它是单点电阻屏控制器，常见坐标命令为 X=`0xD0`、Y=`0x90`，但 SPI mode、字节位序、按下判定和压力命令仍须实测确认。

## 2. 实施边界与推荐文件职责

显示和触摸属于同一块 LCD 模块，底层驱动统一归 `components/LCD` 管理；UI 层只保留 LVGL 输入适配和手势。不要把触摸 SPI 读取、标定公式和 UI 手势全部放进页面代码。建议新增以下文件，并更新 `components/LCD/CMakeLists.txt` 的源文件列表：

```text
components/LCD/
├── lcd.c                    # 现有 ILI9341 显示驱动与 SPI bus 所有者
├── touch.c                  # HR2046 SPI 设备、采样/滤波、按下判断、校准和映射
└── include/
    ├── lcd.h
    └── touch.h              # 触摸配置、初始化、像素坐标读取等公共接口
app/display/lvgl_adapt/
│   └── lv_port_indev.c/.h   # LVGL pointer read_cb 与输入设备注册
app/ui/
    └── ui_gesture.c/.h      # 仅消费 LVGL 事件，判定语义化手势
```

分层规则：

```text
HR2046 原始 ADC / T_IRQ
        ↓  驱动层：采样、滤波、按下判断、校准和旋转
LVGL x/y + PRESSED/RELEASED
        ↓  输入适配层：lv_indev pointer read_cb
LVGL 对象事件（PRESSED / RELEASED）
        ↓  UI 层：手势仲裁、页面/浮层动作
```

UI 层不得理解 `0xD0`、SPI 事务或 raw ADC 值；驱动层不得决定“左滑进入相册”等业务含义。

## 3. 第一步：补齐硬件与总线信息

**当前状态：接线已完成，进入实施测试。** 已确认 LCD 与触摸所需线路已接好；以下 GPIO 分配作为当前测试配置。下一项工作是创建 `components/LCD/touch.c`，完成初始化后只验证原始坐标和按下状态，暂不接入 LVGL 或 UI 手势。

本项目将 `T_CS` 指定为 **GPIO8**，将 `T_IRQ` 指定为 **GPIO18**。当前代码、构建配置和板卡原理图整理均未发现 GPIO8/GPIO18 的其他占用；两者分别引出到 J1-12、J1-11，且不属于启动管脚、下载串口、原生 USB 或板载 RGB LED 引脚。接线为 `LCD 模块 T_CS → ESP32-S3 GPIO8`、`LCD 模块 T_IRQ → ESP32-S3 GPIO18`。接线后必须先完成本节的 raw 读点验证；校准范围仍须实测确认。

| 信号 | 当前可确定的连接 | 仍需确认 |
| --- | --- | --- |
| `T_CLK` | GPIO6（与 LCD SCLK 共用） | 触摸控制器实测 SPI mode 与允许时钟频率。 |
| `T_IN` | GPIO7（与 LCD MOSI 共用） | 模块丝印是否为 `T_IN` 或 `T_DIN`。 |
| `T_DO` | GPIO5（与 LCD MISO 共用） | 未选中时是否释放 MISO。 |
| `T_CS` | **GPIO8**，低有效，由 SPI 设备自动片选 | 将模块 `T_CS` 接至 GPIO8；上电时必须保持高电平（未选中）。 |
| `T_IRQ` | **GPIO18**，输入中断，通常低有效 | 将模块 `T_IRQ` 接至 GPIO18；确认低有效电平、上拉需求和释放后的高电平。 |
| `SD_CS` | 尚未接入 | 若 SD 卡已连线，必须在触摸读取时保持无效。 |

注意：`rgb_led` 的 R/Y/G PWM 引脚分别为 GPIO20/GPIO21/GPIO47，与 LCD SPI 使用的 GPIO5/GPIO6/GPIO7 不冲突；WS2812B 保持 RMT 后端并使用 GPIO48。

实施要求：

1. 保持 LCD 先执行 `lcd_init()`；它已调用 `spi_bus_initialize(SPI2_HOST, ...)`。
2. 在 `components/LCD/touch.c` 的 `touch_init()` 中，只对既有 `SPI2_HOST` 调用 `spi_bus_add_device()` 创建独立的触摸 device handle，配置 `spics_io_num=GPIO8`；不得第二次调用 `spi_bus_initialize()`。
3. 触摸读取期间由 SPI 设备句柄完成片选，禁止手工同时选中 LCD、触摸或 SD 卡。若 `T_CS` 不使用 SPI 驱动自动片选，必须在每次事务前后严格控制电平。
4. 首次将触摸 SPI 时钟设为保守值（例如 1 MHz），验证后再逐步提高；不要沿用 LCD 的 40 MHz。
5. 触摸与显示可以共享总线，但没有共享同一设备句柄。LCD DMA 和触摸事务均应交给 ESP-IDF SPI 主机仲裁；不得绕过驱动直接操作 SPI 寄存器。
6. `T_IRQ` 使用 GPIO18。将其配置为输入和下降沿中断（须由实物确认低有效），并按模块电气要求配置内部或外部上拉；ISR 仅置位标志或通知读取任务。ISR 中不得发 SPI 事务、不得调用 `lv_indev_*` 或其他 LVGL API。第一版也可以保留 GPIO18 接线但按 30 ms 轮询。

验收：上电后 LCD 正常显示，未按下时所有片选均无冲突；触摸驱动初始化日志包含 SPI host、`T_CS=GPIO8`、`T_IRQ=GPIO18`、SPI mode 和频率。

## 4. 第二步：先验证原始读点，不做 UI 映射

在 `components/LCD/touch.c` 实现 `touch_read_raw()`，输出 `raw_x`、`raw_y`、`pressed`，且先不要把这些值发送给 LVGL。读取过程按常见兼容协议准备，但将命令、SPI mode 和结果解包设计为可调整配置。`components/LCD/include/touch.h` 应仅暴露初始化、原始读点、标定设置和“读取已映射像素点”等接口；不得将 SPI device handle 暴露给 UI。

建议的单次采样流程：

1. 判断 `T_IRQ`（若已连接）；其电平只作为“可能按下”的提示。
2. 读取 X、Y 原始 ADC 值；常见命令为 `0xD0` 与 `0x90`，每个结果按 12-bit 解包。
3. 连续采样 5 次；排序后取中位数，或丢弃最大/最小值再平均。先保存未滤波值，便于排障。
4. 用已确认的 `T_IRQ`、Z1/Z2 压力值或“连续样本进入有效范围”给出 `pressed`。仅靠一个随机 ADC 值不能判定按下。
5. 没有按下时返回 `pressed=false`；保留最近一次有效像素坐标供 LVGL 的 release 事件使用。

先在串口每次按下、移动和释放时输出节流后的日志，例如：

```text
touch raw: pressed=1 x=3820 y=214
touch raw: pressed=1 x=2050 y=1965
touch raw: pressed=0
```

在左上、右上、左下、右下和中心各按住 1 秒，记录中位数和样本抖动范围。成功条件如下：

- 同一位置连续采样不出现大幅跳变；若出现，先检查 SPI mode、MISO、供电、片选和采样滤波。
- 移动手指时至少一个 raw 轴单调变化；若完全不变，先检查命令与接线。
- 释放后能可靠变为 `pressed=false`；不能因最后一个 raw 值仍有效而一直“按住”。

## 5. 第三步：以当前显示方向完成校准和有效区域判定

### 5.1 校准的坐标基准

校准目标是“手指按在用户看到的屏幕位置，LVGL 接收到同一位置的像素坐标”。当前显示最终设置为 `PORTRAIT_INVERTED`，不要假设 raw X 对应屏幕 x，也不要预先假设只需翻转 X。

临时校准页应在最终方向下显示五个明显目标点：

```text
(20,20)       (219,20)

              (120,160)

(20,299)      (219,299)
```

依次点击并记录各点的滤波后 `(raw_x, raw_y)`。边距取 20 px，避免电阻屏边缘不可用区干扰；最终 UI 的手势起点和控件热区也应以此有效区域为准。

### 5.2 先确定轴向，再确定标定参数

根据五点记录建立如下关系，而非猜测方向：

| 观察结果 | 映射处理 |
| --- | --- |
| 手指从左到右时 `raw_x` 单调变化，且上/下时 `raw_y` 单调变化 | 不交换轴，分别拟合 X、Y。 |
| 左右移动主要改变 `raw_y` | `swap_xy=true`，将 raw Y 用作屏幕 X。 |
| 映射后左侧被报为右侧 | 对屏幕 X 做镜像：`x = 239 - x`。 |
| 映射后上侧被报为下侧 | 对屏幕 Y 做镜像：`y = 319 - y`。 |

校准顺序必须是：**raw 采样 →（必要时）交换 raw 轴 → 按各轴标定 → 镜像 → 裁剪到显示范围**。这样可把显示旋转与驱动原始坐标明确分开。

若面板近似线性，可先使用每轴最小/最大值：

```text
x = (raw_for_x - raw_x_min) * 239 / (raw_x_max - raw_x_min)
y = (raw_for_y - raw_y_min) * 319 / (raw_y_max - raw_y_min)
```

必须使用 32-bit 或更宽的中间结果，除数为零时返回错误；结果最后限制到 `0..239`、`0..319`。不要用四角读数直接当极值，优先用每个方向上稳定的有效边界读数，并留出少量死区。

若中心点或四边中点明显偏离（例如超过 8 px），采用仿射变换，而不是继续调整 min/max：

```text
x = a * raw_x + b * raw_y + c
y = d * raw_x + e * raw_y + f
```

系数由不少于三个不共线点求解，建议以五点最小二乘拟合；保留标定样本和残差，便于更换模组后复现。是否需要把标定结果写入 NVS 属于后续工作，第一版可先编译期常量，但不可提交未实测的占位值。

### 5.3 校准验收

完成映射后，让校准页显示“raw 点”和“映射后的十字光标”。在五个目标点各触摸至少 10 次：

- 映射点在目标中心 8 px 内；边缘允许略宽，但不得超出屏幕范围。
- 左/右、上/下方向与手指一致；以当前 `PORTRAIT_INVERTED` 的实物画面为准。
- 屏幕四边连续拖动不会突然跳到对侧，也不会因超出 ADC 有效区反复按下/释放。
- 记录最终 `swap_xy`、`mirror_x`、`mirror_y`、有效 raw 范围/仿射系数和测试日期，写入驱动配置注释或专用标定表。

若以后把 `lv_port_disp.c` 的 LCD 方向改为横屏或其他枚举，必须重新运行本节测试。不要只修改 LVGL `hor_res/ver_res`；显示方向和触摸映射必须一起更新。

## 6. 第四步：注册 LVGL pointer 输入设备

仅在原始读点与像素映射均通过第 4、5 节验收后，新增 `lv_port_indev_init()` 并在 `lvgl_port_init()` 中按下面顺序调用：

```text
lv_init()
↓
lv_port_disp_init()             // 注册 240×320 display
↓
touch_init()                    // components/LCD：使用已初始化的 SPI2 总线
↓
lv_port_indev_init()            // 注册 LVGL pointer
↓
启动 tick / LVGL task
↓
在 lvgl_task 内创建 UI
```

输入适配使用 LVGL 8 API：

1. 声明静态 `lv_indev_drv_t`，调用 `lv_indev_drv_init()`。
2. 设置 `type = LV_INDEV_TYPE_POINTER`。
3. 设置 `read_cb`。回调内调用驱动的“读取已滤波像素点”接口，而不是重复实现 raw 映射。
4. 按下时写入 `data->point.x`、`data->point.y` 和 `data->state = LV_INDEV_STATE_PRESSED`。
5. 释放时设为 `LV_INDEV_STATE_RELEASED`，并继续返回最后有效点坐标，避免 release 事件位置未定义。
6. 调用 `lv_indev_drv_register()`，保存返回的 `lv_indev_t *` 供调试和未来配置使用。

`read_cb` 不创建对象、不打印每次轮询日志、不等待 GPIO，不得调用页面切换函数。LVGL 已配置 30 ms 默认读取周期；第一版建议在回调内同步完成一次短 SPI 读取。若后续改为独立采样任务，应使用无锁或受保护的最新点快照给回调读取，仍不可由采样任务操作 LVGL。

最小联调页应包含一个可点击按钮和实时坐标标签。验收为：按下按钮有 `PRESSED` 视觉反馈，松开后只触发一次 `CLICKED`，拖动到按钮外松开不误触发；坐标标签与十字光标位置一致。

## 7. 第五步：在 UI 层校准手势方向与阈值

UI 手势只能在 pointer 输入已经正确映射后启用。`ui_gesture` 在页面根容器记录 `LV_EVENT_PRESSED` 的起点 `(x0,y0)` 与 `LV_EVENT_RELEASED` 的终点 `(x1,y1)`，计算：

```text
dx = x1 - x0
dy = y1 - y0
abs_dx = abs(dx)
abs_dy = abs(dy)
```

以当前交互设计为准：

| 条件 | 语义动作 |
| --- | --- |
| `abs_dx < threshold` 且 `abs_dy < threshold` | 点击，不触发页面手势。 |
| 无浮层、`abs_dx >= threshold`、`abs_dx > abs_dy`、`dx < 0` | 首页左滑进入相册；相册的对应反向动作为右滑返回。 |
| 无浮层、起点 `y0 <= 40`、`dy >= threshold`、`abs_dy > abs_dx` | 从顶部下拉，打开控制中心。 |
| 控制中心可见、`dy <= -threshold`、`abs_dy > abs_dx` | 向上滑，关闭控制中心。 |

初始阈值使用 20 px，但这是待校准的起点而非固定需求。按以下方法确定最终值：

1. 在校准页记录静止按住 1 秒的映射坐标抖动峰峰值 `J`。
2. 在 10 次正常点击中记录最大的非意图位移 `T`。
3. 将初值设为 `max(20, J * 3, T + 5)`，并四舍五入到 5 px；对 240×320 屏幕建议不要超过 40 px，超过时先解决触摸抖动或映射问题。
4. 连续测试左右滑、上下滑、斜滑和滑块拖动各 20 次；记录误判率。若斜滑经常误判，在判定中加入主轴比例，例如 `abs_dx >= abs_dy * 1.2` 或反向条件。

方向判断必须按 **映射后的 LVGL 坐标** 做，不按 raw 坐标做。若用户手指左滑却被 UI 当成右滑，优先回到第 5 节修复坐标镜像；不要在 UI 手势代码中单独反转 `dx`，否则按钮命中与手势方向会相互矛盾。

控件仲裁规则：滑块、开关、按钮优先消费它们自己的事件；从控件起始的拖动不应切换页面或打开控制中心。控制中心显示时禁用页面层的左右滑；设置页显示时不接受页面层和控制中心的手势。

## 8. 实施顺序与完成清单

按以下顺序实施，任一步未通过都不要进入下一步：

1. **确认接线和 GPIO。** 模块 `T_CS` 接至 GPIO8、`T_IRQ` 接至 GPIO18；保持 WS2812B 为 RMT 后端。
2. **创建 LCD 组件内的 HR2046 驱动。** 当前进行：新增 `components/LCD/touch.c` 和 `components/LCD/include/touch.h`，并在既有 SPI2 总线上添加触摸设备，支持可配置 SPI mode、频率和命令。
3. **验证 raw 数据。** 打印按下/移动/释放的滤波前后坐标，解决通信和按下判定问题。
4. **制作临时校准页。** 显示五点、记录 raw 样本，确定轴交换与镜像，得到有效区域和映射参数。
5. **验证像素坐标。** 用十字光标复测五点与边缘拖动；确认当前 `PORTRAIT_INVERTED` 下的方向正确。
6. **接入 LVGL 输入。** 注册一个 `LV_INDEV_TYPE_POINTER`，用按钮和坐标标签验证 press/release/click 链路。
7. **接入 UI 手势。** 实测抖动和点击位移，确定阈值及主轴比例，再启用首页、相册和控制中心动作。
8. **回归测试。** 连续操作 10 分钟，覆盖显示 DMA 刷新、点击、拖动、快速重复点击和控制中心开关；确认无花屏、卡死、MISO 冲突和误触。

建议提交的实测记录至少包括：实际引脚表、SPI mode/频率、五点 raw 数据、最终校准参数、有效像素区域、静止抖动 `J`、手势阈值、方向组合，以及目标板固件版本。

## 9. 常见问题与处理优先级

| 现象 | 优先处理 |
| --- | --- |
| LCD 正常、触摸无 raw 数据 | `T_CS`、T_DO/MISO、SPI mode、命令字节、未选中设备是否释放 MISO。 |
| raw 值不随手指变化或剧烈跳变 | 供电、接地、SPI 频率、滤波、压力/IRQ 判定和 LCD/SD 片选冲突。 |
| 光标左右或上下颠倒 | 先调整驱动层 `swap_xy`/镜像并复测五点，不在 UI 层反转手势。 |
| 只有边缘偏移、中心准确 | 调整有效 raw 区间；若中心也偏离，改用仿射标定。 |
| 点击触发两次或松开不触发 | 检查 `PRESSED/RELEASED` 状态机、最后有效点保留及噪声导致的错误释放。 |
| 拖动滑块切换页面 | 为控件建立事件仲裁，或在手势起点为可交互子对象时取消页面手势。 |
| 加入触摸后花屏或 LCD flush 卡住 | 检查是否重复初始化 SPI2、是否误用 LCD 设备句柄、是否从 ISR/其他任务并发访问 LVGL 或 SPI。 |
