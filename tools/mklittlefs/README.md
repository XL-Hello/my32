# 本项目中的 `mklittlefs`

`mklittlefs` 用于将主机目录制作成 LittleFS 文件系统镜像。本项目通过它将 PNG 图标和 LVGL 二进制字体打包为一个镜像，并烧录到 ESP32-S3 的 `littlefs` 分区。

通常无需单独安装、编译或直接调用本工具；仓库已提供适用于 x86_64 Linux 的压缩包，`build.sh` 会自动解压可执行文件并使用它。

## 项目资源布局

构建 LittleFS 资源时，`build.sh` 使用以下输入：

```text
app/ui/icon/                                      PNG 图标源文件
app/ui/front/list_raw.txt                         UI 控件文案字符列表
app/ui/front/list.txt                             去重后的生成字体字符列表
app/ui/front/Noto-Sans-SC-Bold/NotoSansSCMedium-4.ttf
                                                   UI 字体源文件
```

执行 `./build.sh build flash` 后，脚本先将 `list_raw.txt` 生成无重复字符的单行 `list.txt`，再使用 `lv_font_conv` 生成 12、16、20 px 字体，最后把图标和字体汇集到生成目录：

```text
build/littlefs_assets/
├── fonts/
│   ├── esp_front_12.bin
│   ├── esp_front_16.bin
│   └── esp_front_20.bin
├── humidity.png
├── seticon.png
└── temp.png
```

随后 `mklittlefs` 将该目录打包为：

```text
build/littlefs_icons.bin
```

名称保留了历史兼容性，但镜像现在同时包含图标与字体。

## 分区与文件系统参数

这些参数必须与设备上的 LittleFS 驱动和分区表一致。当前项目在 `build.sh` 中固定为：

| 项目 | 值 |
| --- | --- |
| 分区标签 | `littlefs` |
| Flash 偏移 | `0x600000` |
| 分区/镜像大小 | `0x200000`（2 MiB） |
| LittleFS block size | `4096` 字节 |
| LittleFS page size | `256` 字节 |
| VFS 挂载路径 | `/littlefs` |
| LVGL 文件盘符 | `R:` |

分区表定义位于仓库根目录的 `partitions.csv`。若调整分区大小、block size 或 page size，必须同步修改 LittleFS 驱动配置与 `build.sh`，并重新生成、重新烧录镜像；否则文件系统可能无法挂载。

## 一键生成镜像

在仓库根目录执行：

```bash
./build.sh build flash
```

该命令会：

1. 启用仓库内 `esp-idf/` 并编译应用。
2. 从 `list_raw.txt` 生成去重后的 `list.txt`，再生成三份 LVGL 字体。
3. 解压 `tools/mklittlefs/x86_64-linux-gnu-mklittlefs-42acb97.tar.gz` 到 `build/mklittlefs`（首次或工具包更新时）。
4. 以正确的 LittleFS 参数生成 `build/littlefs_icons.bin`。

不要手动编辑 `list.txt`、`build/littlefs_assets/` 或 `build/littlefs_icons.bin`；它们均为生成物。修改字体字符后，应编辑 `app/ui/front/list_raw.txt`；修改图标后，应编辑 `app/ui/icon/` 下的源文件，然后重新运行上述命令。

## 列出和解包镜像

构建完成后，可检查镜像是否包含图标和三种字体：

```bash
build/mklittlefs -l -b 4096 -p 256 build/littlefs_icons.bin
```

如需解包到临时目录检查文件内容：

```bash
mkdir -p /tmp/my32-littlefs
build/mklittlefs -u /tmp/my32-littlefs -b 4096 -p 256 build/littlefs_icons.bin
find /tmp/my32-littlefs -type f
```

预期至少有以下文件：

```text
fonts/esp_front_12.bin
fonts/esp_front_16.bin
fonts/esp_front_20.bin
```

## 烧录资源镜像

连接开发板后，将 LittleFS 镜像写入 `0x600000` 分区：

```bash
./build.sh flash flash /dev/ttyACM0
```

默认串口也是 `/dev/ttyACM0`，因此可简写为：

```bash
./build.sh flash flash
```

该命令只写入 LittleFS 资源镜像，不会烧录应用固件。若固件和资源都需要更新，依次执行：

```bash
./build.sh build flash
./build.sh flash /dev/ttyACM0
./build.sh flash flash /dev/ttyACM0
```

若串口被监视器占用，关闭占用 `/dev/ttyACM0` 的程序后再执行烧录。不要为了烧录而强制终止用途不明的进程。

## 手动命令参考

项目脚本等价的打包命令如下，通常仅用于诊断：

```bash
build/mklittlefs -c build/littlefs_assets \
  -b 4096 \
  -p 256 \
  -s 0x200000 \
  build/littlefs_icons.bin
```

常用参数：

| 参数 | 含义 |
| --- | --- |
| `-c <目录>` | 从目录创建 LittleFS 镜像 |
| `-u <目录>` | 将镜像解包到目录 |
| `-l` | 列出镜像内文件 |
| `-b <字节>` | LittleFS block size |
| `-p <字节>` | LittleFS page size |
| `-s <字节>` | 镜像总大小 |
| `--version` | 显示工具版本 |

## 常见问题

### 找不到 `build/mklittlefs`

先执行一次：

```bash
./build.sh build flash
```

脚本会从 `tools/mklittlefs/x86_64-linux-gnu-mklittlefs-42acb97.tar.gz` 自动解压该文件。

### 镜像中没有字体

确认 `lv_font_conv` 已安装，且构建命令使用了 `flash` 参数：

```bash
npm i lv_font_conv -g
./build.sh build flash
```

单独执行 `./build.sh build` 不会重建 LittleFS 资源镜像。

### 设备无法挂载 LittleFS

确认已执行 LittleFS 烧录命令，且打包与烧录使用的是同一个 `build/littlefs_icons.bin`。同时检查分区偏移、镜像大小和 block/page 参数是否仍与 `build.sh` 和 `partitions.csv` 一致。
