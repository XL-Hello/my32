# Wi-Fi 设置 UI 模块说明

## 1. 模块概述

`wifi_settings_ui` 是 APP 的 Wi-Fi 设置页面，源码为 [wifi_settings_ui.c](wifi_settings_ui.c)，公开入口为 `wifi_settings_ui_create()` 和 `wifi_settings_ui_create_from_control_center()`。页面运行在固定的 `240 × 320 px` 逻辑显示区域（`LCD_H_RES=240`、`LCD_V_RES=320`），坐标原点为屏幕左上角，`X` 向右增加、`Y` 向下增加。

本文以 [04-Wi-Fi设置.png](../../../../UI稿件/04-Wi-Fi设置.png) 与 [04-Wi-Fi设置-尺寸标注.png](../../../../UI稿件/04-Wi-Fi设置-尺寸标注.png) 为后续重构的实现基线。主页面由顶部导航、Wi-Fi 总开关（C01）、当前连接卡片（C02）、扫描操作区（A01）和纵向滚动热点列表（L01）组成。页面不使用 Flex、Grid 或运行时自适应布局；主页面对象均使用固定像素坐标和固定像素尺寸创建。

调用 `wifi_settings_ui_create()` 或 `wifi_settings_ui_create_from_control_center()` 时，页面会清空当前活动屏幕并创建本页对象。页面定时轮询 `wifi_manager` 状态和扫描结果；离开页面时必须删除该定时器，并销毁密码输入相关的键盘对象。

### 1.1 入口与返回规则

| 入口 | 调用接口 | 返回按钮结果 |
| --- | --- | --- |
| 设置页 | `wifi_settings_ui_create()` | 返回 `settings_ui_create()` |
| 控制中心“网络设置”卡片 | `wifi_settings_ui_create_from_control_center()` | 返回 `controlcenter_ui_create()` |

`controlcenter_ui` 已将“网络设置”卡片的 `LV_EVENT_CLICKED` 绑定到 `wifi_settings_ui_create_from_control_center()`；Wi-Fi 页面不得丢失该来源信息。

### 1.2 全局视觉参数

| 项目 | 固定值 | 实现说明 |
| --- | --- | --- |
| 页面根控件 | 当前活动屏幕 `lv_scr_act()` | 尺寸由显示驱动固定为 `240 × 320 px` |
| 背景色 | `#10201F` | 不透明，`LV_OPA_COVER` |
| 卡片背景 | `#19302E` → `#1B3733` | 从左上向右下的低对比度渐变，不透明 |
| 主交互色 | `#58D6B3` | 开关已启用、扫描、滚动条和主图标 |
| 主文字色 | `#F2FAF7` | 标题、SSID |
| 次级文字色 | `#9BB9B0` | 区块标题和网络详情 |
| 连接状态文字色 | `#7CE3C6` | B01 的“已连接”文字 |
| 卡片圆角 | `7 px` | C01、C02、L01 |
| 默认文字字体 | `ui_font_get_16()` | LittleFS 字体 `R:/littlefs/fonts/esp_front_16.bin` |
| 根控件滚动 | 禁用 | 清除 `LV_OBJ_FLAG_SCROLLABLE`；仅 L01 可滚动 |

### 1.3 LittleFS 资源规则

界面 PNG 源文件位于 `app/ui/icon/png/`。执行 `./build.sh build flash` 会重新生成 `build/littlefs_icons.bin`，其中包含 `/png/` 与 `/fonts/` 资源；该命令不执行物理烧录。运行时由盘符 `R:` 访问已挂载的 LittleFS，路径统一为 `R:/littlefs/png/<文件名>`。

| 用途 | 设计源文件 | 应用 PNG 源文件 | 运行时路径 | 原始 / 展示尺寸 | 显示规则 |
| --- | --- | --- | --- | --- | --- |
| 返回 | `UI稿件/icons/back.svg` | `app/ui/icon/png/back.png` | `R:/littlefs/png/back.png` | `24 × 24` / `24 × 24 px` | 主色重着色、1:1 显示 |
| 强信号 | `UI稿件/icons/wifi-signal-strong.svg` | `app/ui/icon/png/wifi-signal-strong.png` | `R:/littlefs/png/wifi-signal-strong.png` | `24 × 24` / `20 × 20 px` | 当前连接、强信号热点 |
| 中信号 | `UI稿件/icons/wifi-signal-medium.svg` | `app/ui/icon/png/wifi-signal-medium.png` | `R:/littlefs/png/wifi-signal-medium.png` | `24 × 24` / `20 × 20 px` | 中信号热点 |
| 弱信号 | `UI稿件/icons/wifi-signal-low.svg` | `app/ui/icon/png/wifi-signal-low.png` | `R:/littlefs/png/wifi-signal-low.png` | `24 × 24` / `20 × 20 px` | 弱信号热点 |
| 扫描 | `UI稿件/icons/refresh.svg` | `app/ui/icon/png/refresh.png` | `R:/littlefs/png/refresh.png` | `24 × 24` / `16 × 16 px` | 主色重着色 |
| 加密网络 | `UI稿件/icons/lock.svg` | `app/ui/icon/png/lock.png` | `R:/littlefs/png/lock.png` | `24 × 24` / `16 × 16 px` | 次级色重着色 |

图像均通过 `ui_icon_set_src()` 加载，随后使用 `img_recolor` 着色，并清除 `LV_OBJ_FLAG_CLICKABLE`。所有缩小显示的图标均保留 `24 × 24 px` 源图对象、按展示尺寸设置 `img_zoom`，并以源图中心缩放，使可见图形严格居中于设计坐标框。主页面中的交互事件必须挂在卡片、按钮或列表项容器上，不能挂在图像对象上。

## 2. 顶部导航

### 2.1 H01 返回按钮

| 属性 | 固定参数 |
| --- | --- |
| 控件类型 | 透明 `lv_btn`，内部包含 `lv_img` |
| 建议命中区 | `(8, 8)`，`40 × 40 px` |
| 可见圆形边框 | 中心 `(28, 28)`，直径 `23 px` |
| 内部图标 | `back.png`，左上角 `(16, 16)`，`24 × 24 px` |
| 样式 | 透明背景；`1 px` 主色边框；圆形；无阴影、无内边距 |
| 事件 | `LV_EVENT_CLICKED`；按入口来源返回控制中心或设置页 |

### 2.2 H02 页面标题

| 属性 | 固定参数 |
| --- | --- |
| 控件类型 | `lv_label` |
| 固定文案 | `Wi-Fi 设置` |
| 文字框左上角 | `(54, 19)` |
| 文字框尺寸 | `83 × 19 px` |
| 字体 | `ui_font_get_16()`，`15 px`，字重 600 |
| 颜色 / 样式 | `#F2FAF7`；单行；无边框、无内边距 |

## 3. Wi-Fi 状态与操作区

### 3.1 C01 Wi-Fi 总开关卡片

C01 展示 Wi-Fi 启用状态；其高度固定为 `46 px`，不得恢复为旧版的 `64 px`。开关必须使用真实的可点击控件，并通过 `wifi_manager_set_enabled()` 改变状态。

| 组件 | 控件类型 | 左上角坐标 | 尺寸 | 资源 / 文案 | 样式与行为 |
| --- | --- | ---: | ---: | --- | --- |
| 卡片背景 | `lv_obj` | `(12, 54)` | `216 × 46 px` | 无 | 渐变卡片；圆角 `7 px`；不可滚动 |
| 图标底座 | `lv_obj` | `(24, 64)` | `26 × 26 px` | 无 | `#1D4A43` 圆形背景 |
| Wi-Fi 图标 | `lv_img` | `(27, 67)` | `20 × 20 px` | `wifi-signal-strong.png` | 主色；不可点击 |
| 主标题 | `lv_label` | `(58, 61)` | 内容尺寸 `38 × 17 px` | `Wi-Fi` | `14 px`、字重 600、主文字色 |
| 状态说明 | `lv_label` | `(58, 80)` | 内容尺寸 `126 × 11 px` | 启用时为 `已开启 · 自动加入已知网络` | `9 px`、次级文字色；关闭时按状态改写 |
| U01 总开关 | `lv_switch` | `(184, 68)` | `32 × 18 px` | 无 | 启用态轨道为主色、旋钮直径 `14 px`；透明扩展命中区 `(174, 58)`、`52 × 38 px` |

### 3.2 S01 与 C02 当前连接

| 组件 | 控件类型 | 左上角坐标 | 尺寸 | 资源 / 文案 | 样式与行为 |
| --- | --- | ---: | ---: | --- | --- |
| S01 区块标题 | `lv_label` | `(16, 104)` | 内容尺寸 `55 × 14 px` | `当前连接` | `11 px`、字重 600、次级文字色 |
| C02 卡片背景 | `lv_obj` | `(12, 123)` | `216 × 46 px` | 无 | 渐变卡片；圆角 `7 px`；可点击，进入当前网络详情 |
| 图标底座 | `lv_obj` | `(24, 133)` | `26 × 26 px` | 无 | `#1D4A43` 圆形背景 |
| Wi-Fi 图标 | `lv_img` | `(27, 136)` | `20 × 20 px` | `wifi-signal-strong.png` | 主色；不可点击 |
| 当前 SSID | `lv_label` | `(58, 129)` | 内容尺寸，最大宽度 `118 px` | `wifi_manager_network_info_t.ssid` | `14 px`、字重 600、主文字色；超长文本省略 |
| 网络详情 | `lv_label` | `(58, 148)` | 内容尺寸，最大宽度 `118 px` | 已连接时为 `网络稳定 · <IPv4>` | `9 px`、次级文字色；按状态动态更新 |
| B01 连接状态 | `lv_label` | `(184, 133)` | `32 × 12 px` | `已连接` | `9 px`、字重 600、`#7CE3C6`；仅文字，不得绘制勾选图标或状态底板 |

未连接或 Wi-Fi 已关闭时，C02 仍保留固定位置和尺寸；SSID、详情和 B01 使用 `wifi_manager_status_to_text()` 或无连接文案更新，不能继续显示已连接状态。

### 3.3 S02 与 A01 扫描操作区

扫描操作区独立于列表和标题，不得与任意控件重叠。图标和文字必须以 `Y=189 px` 为共同视觉中心。

| 组件 | 控件类型 | 左上角坐标 | 尺寸 | 资源 / 文案 | 样式与行为 |
| --- | --- | ---: | ---: | --- | --- |
| S02 区块标题 | `lv_label` | `(16, 181)` | 内容尺寸 `55 × 14 px` | `可用网络` | `11 px`、字重 600、次级文字色 |
| A01 扫描按钮 | `lv_btn` | `(172, 177)` | `56 × 24 px` | 无 | 透明深色背景、`0.7 px` 交互色边框、圆角 `5 px` |
| 扫描图标 | `lv_img` | `(179, 181)` | `16 × 16 px` | `refresh.png` | 主色、不可点击；与文字组成 `42 × 16 px` 的居中内容组 |
| 扫描文字 | `lv_label` | `(199, 182)` | 内容尺寸 `22 × 14 px` | `扫描` | `11 px`、字重 600、主交互色 |

A01 收到 `LV_EVENT_CLICKED` 时调用 `wifi_manager_start_scan()`。扫描期间按钮保持可见，可禁用重复点击或显示扫描中状态；列表内容改为扫描提示，扫描结束后读取 `wifi_manager_get_scan_results()` 刷新 L01。

## 4. L01 可用网络滚动列表

L01 是页面内唯一允许纵向滚动的对象。其可视视窗固定为 `216 × 100 px`，内部内容高度由实际热点数量决定；不得为适应热点数量压缩单项高度或覆盖 A01。

### 4.1 列表容器与滚动条

| 组件 | 控件类型 | 左上角坐标 | 尺寸 | 样式 / 行为 |
| --- | --- | ---: | ---: | --- |
| L01 列表视窗 | 可滚动 `lv_obj` | `(12, 210)` | `216 × 100 px` | 渐变卡片、圆角 `7 px`、纵向滚动、横向滚动禁用 |
| 内容区域 | L01 内部内容区 | `(12, 210)` 起 | 最小高度 `100 px` | 每个热点项固定 `40 px` 高；内容超过视窗时允许向上滑动 |
| 分隔线 | `lv_line` 或边框 | `X=24`，每项底部 | `192 × 1 px` | `#31534E`、`0.65 px` |
| 滚动条轨道 | `LV_PART_SCROLLBAR` | 右侧 | `2 × 84 px` 视觉规格 | `#264A44`，圆角 `1 px`；始终可见 |
| 滚动条滑块 | `LV_PART_SCROLLBAR` | 随位置变化 | 初始视觉高度 `34 px` | `#58D6B3`，圆角 `1 px` |

### 4.2 热点项通用结构

| 属性 | 固定参数 |
| --- | --- |
| 单项控件类型 | 可点击 `lv_obj` 或无默认样式的 `lv_btn` |
| 单项尺寸 | `216 × 40 px` |
| 内边距 | 左右 `12 px`；不使用额外纵向内边距 |
| SSID | 左上角相对坐标 `(12, 3)`；`13 px`、字重 500、主文字色；最长 `136 px`，超出省略 |
| 安全说明 | 左上角相对坐标 `(12, 22)`；`8 px`、次级文字色；受保护网络显示 `需要密码`，开放网络显示 `开放网络` |
| 加密图标 | 左上角相对坐标 `(151, 12)`；`16 × 16 px`；仅受保护网络显示 |
| 信号图标 | 左上角相对坐标 `(180, 8)`；`20 × 20 px`；根据 RSSI 切换强、中、弱资源 |
| 点击行为 | 受保护网络打开密码输入；开放网络以空密码直接调用 `wifi_manager_connect()` |

| 示例项 | 左上角坐标 | 尺寸 | 可见内容 |
| --- | ---: | ---: | --- |
| N01 | `(12, 210)` | `216 × 40 px` | `Home_5G`、`需要密码`、锁、强信号 |
| N02 | `(12, 250)` | `216 × 40 px` | `Guest`、`开放网络`、中信号 |
| N03 | `(12, 290)` | `216 × 40 px` | `Studio_2.4G`、锁、弱信号；视窗仅露出顶部，提示可继续滑动 |

## 5. 动态状态、密码输入与资源生命周期

### 5.1 状态刷新

页面以 `300 ms` 周期读取 `wifi_manager_get_network_info()`，并同步以下内容：

1. U01 的选中状态与 `info.enabled` 一致；禁止仅更新视觉而不调用 `wifi_manager_set_enabled()`。
2. C01 状态说明在启用、关闭、扫描、连接失败等状态下更新。
3. C02 的 SSID、IPv4 详情与 B01 按 `info.status` 和 `info.ipv4` 更新；只有可用连接状态才显示“已连接”。
4. `s_waiting_for_scan` 为真且 `wifi_manager_is_scanning()` 变为假时，读取扫描缓存并重建 L01 项目。
5. RSSI 建议分级：大于等于 `-55 dBm` 为强信号，`-70 dBm` 至 `-56 dBm` 为中信号，小于等于 `-71 dBm` 为弱信号。
6. Wi-Fi 关闭成功后，立即清空扫描结果、扫描等待状态及 L01 的全部子项；列表不得保留热点、滚动条或“未发现可用热点”等旧状态。

### 5.2 连接与密码输入

主页面不常驻显示密码输入框、连接、断开或忘记按钮。点击受保护热点后，创建临时密码输入层；点击开放热点后，直接以空密码调用 `wifi_manager_connect()`。密码层必须支持：

- 显示当前选中 SSID；
- 单行密码输入和显示/隐藏密码；
- `连接`、`取消` 操作；
- `LV_EVENT_READY` 时提交连接，`LV_EVENT_CANCEL` 时关闭键盘；
- 密码长度只允许为空或 `8` 至 `63` 字节，并在调用失败时显示错误提示。

点击 C02 后进入当前网络操作层，提供 `断开` 和 `忘记`；分别调用 `wifi_manager_disconnect()` 与 `wifi_manager_forget_network()`。该层关闭后必须返回主页面，不得删除主页面的刷新定时器。

### 5.3 页面销毁

离开 Wi-Fi 页面前必须按以下顺序处理：

1. 删除 `s_refresh_timer` 并清空指针；
2. 销毁或隐藏密码层、键盘和当前网络操作层；
3. 依据入口来源调用 `controlcenter_ui_create()` 或 `settings_ui_create()`；
4. 不得在 `lv_obj_clean()` 后继续访问已销毁的页面对象指针。

## 6. 字体、资源打包与验证

1. 新增或修改任何中文文案后，必须同步更新 `app/ui/front/list_raw.txt`；执行 `python3 app/ui/front/generate_font_list.py` 生成去重后的 `list.txt`。
2. 新增页面图标后，必须将对应透明 PNG 放入 `app/ui/icon/png/`，在 `icon.h` 中增加 `R:/littlefs/png/` 路径宏，并使用 `ui_icon_set_src()` 加载。
3. 运行 `./build.sh build` 验证 C 代码；运行 `./build.sh build flash` 生成 LittleFS 资源镜像。生成镜像与真实设备烧录是独立操作，未经明确授权不得执行 `./build.sh flash`。
4. 检查 `git diff --check`，并在目标设备上验证：控制中心直达、两种返回路径、U01、A01、纵向滚动、受保护网络密码输入、开放网络直连和 C02 网络操作。
