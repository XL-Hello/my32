# 使用 `lv_font_conv` 制作并烧录 LVGL 字体

本文说明本项目如何将控件文案中的字符转换为 LVGL 二进制字体，和 UI 图标一起写入 LittleFS 分区，并由 LVGL 从 Flash 加载显示。

## 工作方式

项目不再将中文字体编译为 C 数组并链接进应用固件。执行 `./build.sh build flash` 时，脚本会完成以下工作：

1. 将 `list_raw.txt` 中的控件文案去重生成 `list.txt`，再使用 `NotoSansSCMedium-4.ttf` 和该字符列表生成 12、16、20 px 三个 LVGL 二进制字体。
2. 将三个字体和 `app/ui/icon/` 下的 PNG 图标一起打包为 `build/littlefs_icons.bin`。
3. 该镜像对应分区表中偏移为 `0x600000`、大小为 2 MiB 的 `littlefs` 分区。
4. 设备启动时先挂载 `/littlefs`，随后 LVGL 使用 `lv_font_load()` 从 `R:/littlefs/fonts/` 加载字体。

资源在 LittleFS 中的路径如下：

```text
fonts/esp_front_12.bin
fonts/esp_front_16.bin
fonts/esp_front_20.bin
humidity.png
seticon.png
temp.png
```

`R:` 是 LVGL 的文件系统盘符，实际映射到 ESP-IDF 挂载路径 `/littlefs`。

## 前置条件

- 使用仓库内的 ESP-IDF；不要加载全局 ESP-IDF 的 `export.sh`。
- 已安装 Node.js 14 或更高版本。
- 可访问 npm 软件源以首次安装 `lv_font_conv`。

检查 Node.js 版本：

```bash
node --version
npm --version
```

## 安装字体转换工具

在任意终端执行一次：

```bash
npm i lv_font_conv -g
```

安装后确认工具可用：

```bash
lv_font_conv --version
lv_font_conv --help
```

如果 `build.sh` 提示未安装 `lv_font_conv`，重新执行上述安装命令，并确认全局 npm 可执行目录已在 `PATH` 中。

## 维护字符列表

控件文案的原始字符列表为：

```text
app/ui/front/list_raw.txt
```

其中应包含所有会由 12、16 或 20 px UI 字体显示的字符，包括中文、英文、数字、标点及符号。可按控件、页面或文案逐行记录，允许重复字符。每次修改界面文案后，应将新增文案补充到该文件，再重新生成 LittleFS 镜像。

`generate_font_list.py` 会遍历原始文本，移除换行并按首次出现顺序去重，生成以下单行文件：

```text
app/ui/front/list.txt
```

`list.txt` 是生成物，不应手动编辑。需要单独更新时，可执行：

```bash
python3 app/ui/front/generate_font_list.py
```

例如，若界面新增文案“设备已重启”，将该文案加入 `list_raw.txt`：

```text
设备已重启
```

脚本会自动去除重复字符以减小 Flash 占用。原始列表中必须保留现有 ASCII 字符集；Wi-Fi 密码输入、数字状态和英文 SSID 等界面需要它们。

字体源文件固定为：

```text
app/ui/front/Noto-Sans-SC-Bold/NotoSansSCMedium-4.ttf
```

当前生成参数与原有 UI 字体保持一致：4 bpp、12/16/20 px、无压缩。无压缩格式可与项目当前 LVGL 二进制字体加载器兼容。

## 生成应用与资源镜像

在仓库根目录执行：

```bash
./build.sh build flash
```

该命令会先更新 `list.txt`，再使用项目内 `esp-idf/` 编译应用并创建以下文件：

```text
build/littlefs_icons.bin
```

构建过程中的临时资源目录为 `build/littlefs_assets/`，其中可查看本次生成的 `.bin` 字体。该目录是生成物，不要手动维护；下次 `build flash` 会重新创建它。

仅编译应用、不重新生成字体资源时使用：

```bash
./build.sh build
```

## 检查生成结果

可列出 LittleFS 镜像内容：

```bash
build/mklittlefs -l -b 4096 -p 256 build/littlefs_icons.bin
```

预期能看到：

```text
fonts/esp_front_12.bin
fonts/esp_front_16.bin
fonts/esp_front_20.bin
```

也可检查临时目录中的文件大小：

```bash
du -h build/littlefs_assets/fonts/*.bin build/littlefs_icons.bin
```

## 烧录 LittleFS 资源

连接 ESP32-S3 后，将图标和字体镜像写入 LittleFS 分区：

```bash
./build.sh flash flash /dev/ttyACM0
```

不传串口时，脚本使用默认值 `/dev/ttyACM0`：

```bash
./build.sh flash flash
```

该命令只烧录 `build/littlefs_icons.bin` 到 `0x600000`，不会重刷应用固件。需要同时烧录应用时，先构建资源，再分别执行：

```bash
./build.sh build flash
./build.sh flash /dev/ttyACM0
./build.sh flash flash /dev/ttyACM0
```

## LVGL 字体加载接口

字体管理模块位于：

```text
app/ui/front/ui_font.c
app/ui/front/ui_font.h
```

在 LittleFS 已挂载且 LVGL 已初始化后，`ui_font_init()` 会加载三种字体：

```c
lv_font_load("R:/littlefs/fonts/esp_front_16.bin");
```

UI 中通过以下接口取得字体，而不是引用旧的 `esp_front_*.c` 静态变量：

```c
lv_obj_set_style_text_font(label, ui_font_get_12(), LV_PART_MAIN);
lv_obj_set_style_text_font(label, ui_font_get_16(), LV_PART_MAIN);
lv_obj_set_style_text_font(label, ui_font_get_20(), LV_PART_MAIN);
```

新增 UI 文本时，应根据实际字号选择对应接口。`ui_font_init()` 失败时不会创建主 UI，以避免将空字体指针传给 LVGL；请检查 LittleFS 是否已烧录，以及镜像内字体路径是否完整。

## 常见问题

### `lv_font_conv: command not found`

执行：

```bash
npm i lv_font_conv -g
```

若安装成功但命令仍不可用，重新打开终端或将 npm 全局 bin 目录加入 `PATH`。

### 界面出现方框或缺字

将缺失文案或字符加入 `app/ui/front/list_raw.txt`，然后重新执行：

```bash
./build.sh build flash
./build.sh flash flash /dev/ttyACM0
```

### LittleFS 镜像超出分区容量

当前 LittleFS 分区大小为 2 MiB。优先删除 `list_raw.txt` 中不再使用的文案；如果仍不足，需要在修改 `partitions.csv` 后评估 Flash 分区布局和应用空间。

### 串口提示 `Resource temporarily unavailable`

串口正在被监视器或其他进程占用。关闭占用 `/dev/ttyACM0` 的串口工具后，再重新执行 LittleFS 烧录命令。不要强制终止不确定用途的进程。
