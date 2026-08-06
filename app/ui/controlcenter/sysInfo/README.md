# 系统信息与本地 HTTP OTA UI 模块说明

## 1. 模块概述

`sys_info_ui` 是控制中心“系统信息”卡片的二级页面。页面运行在固定的 `240 × 320 px` 逻辑显示区域（`LCD_H_RES=240`、`LCD_V_RES=320`），坐标原点为屏幕左上角，`X` 向右增加、`Y` 向下增加。实现位于同目录的 `sys_info_ui.c` 与 `sys_info_ui.h`；`local_ota` 通过可轮询的状态快照提供分阶段检查、下载校验与安装流程，OTA 工作任务不直接操作 LVGL。

页面展示三项设备信息与一个软件更新操作区：**本机型号**、**OS 版本**、**机身内存**、**更新检测**。前三项在首版使用默认值填充，视觉结构统一为“`24 × 24 px` 图标 + `14 px` 大标题 + `9 px` 小文字说明”。更新操作区采用明确状态机，避免用户首次点击就写入 OTA 分区。

入口流程为：控制中心“系统信息”卡片点击后调用规划入口 `sys_info_ui_create()`；页面返回按钮回到 `controlcenter_ui_create()`。不创建底部上滑返回手势区，以免在下载和安装确认状态下误触离开页面。

### 1.1 全局视觉参数

| 项目 | 固定值 | 实现说明 |
| --- | --- | --- |
| 页面根控件 | 当前活动屏幕 `lv_scr_act()` | 尺寸由显示驱动固定为 `240 × 320 px` |
| 背景色 | `#10201F` | 不透明，`LV_OPA_COVER` |
| 卡片背景 | `#19302E` → `#1B3733` | 水平或左上到右下的轻微渐变；不透明 |
| 卡片边框 | `#29524C`，`0.7 px` | 更新发现 / 可安装状态改为 `#58D6B3`，`0.9 px` |
| 页面标题 / 大标题 | `#F2FAF7` | 标题 `16 px`，卡片大标题 `14 px` |
| 小文字说明 | `#9BB9B0` | `9 px`，单行，不换行 |
| 主交互与图标 | `#58D6B3` | 所有 PNG 通过 `img_recolor` 着色 |
| 成功状态文字 | `#7CE3C6` | “发现新版本”“可安装”和进度百分比 |
| 根控件滚动 | 禁用 | 清除 `LV_OBJ_FLAG_SCROLLABLE` |

### 1.2 稿件与 LittleFS 资源规则

设计源稿位于 `UI稿件/`，实现使用的 PNG 源文件应复制或导出至 `app/ui/icon/png/`。执行 `./build.sh build flash` 后，脚本将 PNG 打入 LittleFS 的 `/png/` 目录；LVGL 运行时资源路径统一为 `R:/littlefs/png/<文件名>`。

| 用途 | 设计资源 | 建议 PNG 文件名 | 运行时路径 | 原始 / 展示尺寸 |
| --- | --- | --- | --- | --- |
| 返回 | `UI稿件/icons/back.svg` | `back.png` | `R:/littlefs/png/back.png` | `24 × 24 / 24 × 24 px` |
| 本机型号 | `UI稿件/icons/chip.svg` | `chip.png` | `R:/littlefs/png/chip.png` | `24 × 24 / 24 × 24 px` |
| OS 版本 | `UI稿件/icons/os.svg` | `os.png` | `R:/littlefs/png/os.png` | `24 × 24 / 24 × 24 px` |
| 机身内存 | `UI稿件/icons/storage.svg` | `storage.png` | `R:/littlefs/png/storage.png` | `24 × 24 / 24 × 24 px` |
| 检查更新 | `UI稿件/icons/update.svg` | `update.png` | `R:/littlefs/png/update.png` | `24 × 24 / 24 × 24 px` |
| 下载 | `UI稿件/icons/download.svg` | `download.png` | `R:/littlefs/png/download.png` | `24 × 24 / 24 × 24 px` |
| 安装 | `UI稿件/icons/install.svg` | `install.png` | `R:/littlefs/png/install.png` | `24 × 24 / 24 × 24 px` |
| 右箭头 | `UI稿件/icons/chevron-right.svg` | `chevron-right.png` | `R:/littlefs/png/chevron-right.png` | `24 × 24 / 24 × 24 px` |

图标对象均使用 `lv_img`，原始尺寸显示，不调用 `lv_img_set_zoom()`，并清除 `LV_OBJ_FLAG_CLICKABLE`。点击事件只挂在完整卡片对象上。

## 2. 全局导航与顶部控件

### 2.1 返回控制中心按钮

| 属性 | 固定参数 |
| --- | --- |
| 功能 | 接收 `LV_EVENT_CLICKED` 后调用 `controlcenter_ui_create()` 返回控制中心 |
| 控件类型 | `lv_btn`，内部包含 `lv_img` |
| 按钮左上角坐标 / 尺寸 | `(16, 16)` / `24 × 24 px` |
| 视觉样式 | 透明背景、`1 px` 边框 `#58D6B3`、圆形 `LV_RADIUS_CIRCLE`、无阴影、无内边距 |
| 图标 | `back.png`，同按钮大小居中，重着色 `#58D6B3` |

下载中禁止因返回按钮中断任务：点击返回只离开页面，后台下载仍由 OTA 模块完成并保留状态；首版若不支持后台状态持久化，应在下载状态隐藏或禁用返回按钮，二者只能选择其一并在代码中明确。

### 2.2 页面标题

| 属性 | 固定参数 |
| --- | --- |
| 控件类型 | `lv_label` |
| 固定文案 | `系统信息` |
| 左上角坐标 | `(54, 17)` |
| 字体 / 颜色 | `ui_font_get_16()` / `#F2FAF7` |
| 内容尺寸 | `64 × 18 px`；单行；无边框、无内边距 |

## 3. 固定信息卡片

三张信息卡片均为不可点击 `lv_obj`，尺寸 `216 × 43 px`，左边界 `X=12`，圆角 `7 px`，无内边距、无滚动。每张卡片左侧都固定使用直径 `26 px` 的深色圆形图标底座（圆心 `X=37`），图标本体为 `24 × 24 px`；右侧统一为大标题与小文字说明。

| 卡片 | 左上角 | 图标底座 / 图标左上角 | 大标题左上角 | 小文字基线 | 默认内容 |
| --- | ---: | ---: | ---: | ---: | --- |
| C01 本机型号 | `(12, 61)` | `(24, 69.5)` / `(25, 70.5)` | `(58, 66)` | `Y=94` | `Dreame ESP32-S3` |
| C02 OS 版本 | `(12, 112)` | `(24, 120.5)` / `(25, 121.5)` | `(58, 117)` | `Y=145` | `v1.0.0 · 当前稳定版` |
| C03 机身内存 | `(12, 163)` | `(24, 171.5)` / `(25, 172.5)` | `(58, 168)` | `Y=196` | `8 MB Flash · 2 MB LittleFS` |

大标题使用 `ui_font_get_16()`（设计字号 `14 px` 的视觉等级，实际实现以项目字库可用字形为准）；小文字使用 `ui_font_get_12()` 或提供 `9 px` 字库后再替换。默认值必须封装为可替换的数据接口，后续接入芯片型号、编译版本和分区信息时不得修改布局代码。

## 4. 软件更新操作区

### 4.1 更新卡片通用结构

| 属性 | 固定参数 |
| --- | --- |
| 分组标题 | `软件更新`，左上角 `(16, 216)`，`10 px`，`#9BB9B0` |
| A01 卡片 | `(12, 233)`，`216 × 66 px`，圆角 `7 px` |
| 点击热区 | 整张 A01 卡片；仅状态允许时注册 `LV_EVENT_CLICKED` |
| 图标底座 / 图标左上角 | `(24, 253)` / `(25, 254)`；`26 × 26 px` / `24 × 24 px` |
| 主文字 | `(58, 247)`，`14 px`；状态决定文案和颜色 |
| 辅助文字 | `(58, 276)` 的 `9 px` 单行文本；显示引导或 OTA 文件名 |
| 右侧元素 | 非下载状态放置 `chevron-right.png` 于 `(202, 254)`；下载状态替换为百分比与进度条 |

### 4.2 更新状态机

| 状态 | A01 主文字 / 小文字 | 用户点击 | 后台动作 | 页面反馈 |
| --- | --- | --- | --- | --- |
| `NO_UPDATE` | `更新检测` / `无更新 · 轻触检查` | 允许 | HTTP 访问配置的完整固件 URL，检查状态码与长度 | 检查期间可显示短暂转圈；文件不存在时回到本状态 |
| `UPDATE_AVAILABLE` | `发现新版本` / `<ota 文件名> · 轻触下载` | 允许 | 不写 OTA 分区 | 主文字改为 `#7CE3C6`；显示文件名 |
| `DOWNLOADING` | `正在下载` / `<ota 文件名>` | 禁止 | HTTP `GET <package_url>`；分块读、长度校验、写入候选 OTA 分区 | 显示 `0–100%` 与进度条；不可重复发起 |
| `READY_TO_INSTALL` | `可安装` / `<ota 文件名> · 轻触安装` | 允许 | 不立即重启 | 主文字改为 `#7CE3C6`，恢复右箭头 |
| `INSTALLING` | `正在升级` / `请勿断电` | 禁止 | 调用既有 OTA 写入、镜像校验、切换启动分区和重启流程 | 进入既有转圈升级遮罩，直至重启 |
| `FAILED` | `更新失败` / `<失败原因> · 轻触重试` | 允许 | 清理临时下载状态 | 使用警告色，点击回到对应可重试步骤 |

`DOWNLOADING` 状态的进度定义为 `received_bytes / Content-Length × 100`，不得以 HTTP 回调次数估算。进度条轨道为 `(58, 281)`、`146 × 5 px`、圆角 `2.5 px`、颜色 `#274844`；填充宽度为 `146 × progress / 100`，颜色 `#58D6B3`；百分比文字右对齐于 `X=208`。

## 5. HTTP 与 OTA 对接约束

1. 设备端 `CONFIG_LOCAL_OTA_HTTP_PACKAGE_URL` 必须填写电脑在局域网中的完整固件地址，例如 `http://192.168.31.12:8080/hello_world.bin`。`127.0.0.1` 只表示 ESP32 自身，不能访问电脑上运行的本地服务。
2. 更新检查只允许在用户点击 A01 且状态为 `NO_UPDATE` 时发起；禁止在开机、页面创建或定时器中自动检查。
3. 检查接口直接访问 `CONFIG_LOCAL_OTA_HTTP_PACKAGE_URL`；只有 HTTP 状态为 `200` 且 `Content-Length > 0` 时，才能切换为 `UPDATE_AVAILABLE`。`404` 表示无更新包。
4. 下载接口与检查接口使用同一完整 URL。必须验证 HTTP 状态为 `200`、两次 `Content-Length` 一致、下载总长度一致，并执行 ESP 镜像校验；任一失败都不得切换启动分区。
5. 每处理 `16 KiB` 数据调用一次任务让出；当前系统 tick 分辨率可能使 `2 ms` 延时向上取整，界面进度刷新应通过安全的 UI 消息或定时器回调在 LVGL 线程完成，禁止在 OTA 任务中直接操作 LVGL 对象。
6. 安装动作只能发生在 `READY_TO_INSTALL` 点击后。写入成功后再执行 `esp_ota_set_boot_partition()`，并立即 `esp_restart()`；新固件启动后仍按既有回滚确认流程调用 `local_ota_confirm_running_app()`。

## 6. 实现接口与维护要点

当前 `sys_info_ui.c` 与 `sys_info_ui.h` 提供：

```c
void sys_info_ui_create(void);
void sys_info_ui_update_progress(size_t received_bytes, size_t total_bytes);
void sys_info_ui_set_update_state(sys_info_update_state_t state,
                                  const char *file_name,
                                  const char *detail);
```

1. 控制中心“系统信息”卡片必须在卡片对象上注册 `LV_EVENT_CLICKED` 并调用 `sys_info_ui_create()`；不能复用当前点击即 `local_ota_start()` 的行为。
2. `local_ota` 已拆分为“检查清单”“下载并校验”“确认安装”三段异步接口；UI 通过状态快照轮询更新显示，不解析 JSON、执行 HTTP 或直接调用 `esp_ota_write()`。下载阶段写入候选 OTA 分区，但仅在确认安装后切换启动分区。
3. 页面销毁时必须删除页面定时器，并解除 OTA 状态回调；不得保留已释放的 `lv_obj_t *`。
4. 新增 PNG 后执行 `./build.sh build flash` 重新生成 `build/littlefs_icons.bin`；仅烧录应用分区不会更新图标资源。
5. 实现完成后应同步更新本文档中的真实源文件名、接口名、字体尺寸以及所有实测坐标；设计稿对应 `UI稿件/07-系统信息-OTA-尺寸标注.svg`。
