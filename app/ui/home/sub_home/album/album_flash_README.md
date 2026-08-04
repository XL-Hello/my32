# 相册 Flash 图片资源模块说明

## 1. 模块概述

`album_flash` 是相册图片资源的唯一底层访问模块，源码为 [album_flash.c](album_flash.c)，公开接口为 [album_flash.h](album_flash.h)。模块只维护两项页面所需状态：每次初始化遍历文件得到的图片总数，以及当前索引对应的图片数据。它负责枚举 LittleFS 中已打包的相册 PNG、建立稳定索引、读取当前图片数据和长度，并向上层输出只读图片字节数组。

模块边界固定为：源资源只允许来自 `app/ui/home/sub_home/album/photo/`；运行时只允许访问 LittleFS 中由该目录整体打包得到的 `/photo/`；不读取 SD 卡、不接受其他文件路径、不创建 LVGL 控件。`home_album` 与 `album` 只能通过本模块取得图片数组，不能直接遍历 Flash 或读取图片文件。

本文只定义资源、打包和接口契约，不嵌入、列出或引用任何具体相册图片。

### 1.1 资源路径与打包目标

| 阶段 | 允许位置 | 内容 | 规则 |
| --- | --- | --- | --- |
| 源资源 | `app/ui/home/sub_home/album/photo/` | 仅当前目录下的 `*.png` | 目录外文件、子目录、JPG/JPEG/BMP/GIF/SVG 等均不得参与相册打包 |
| LittleFS staging | `build/littlefs_assets/photo/` | 将源目录全部合格 PNG 原样复制 | 文件名必须保持不变；不得混入图标或字体 |
| LittleFS 镜像 | `/photo/` | 完整相册 PNG 集合 | 与 staging 目录一一对应 |
| 运行时 | `R:/littlefs/photo/<文件名>.png` | 只读相册数据 | 路径只可在 `album_flash.c` 内部构造和使用 |

现有 `build.sh` 的 `build_littlefs_assets()` 目前只创建并填充 `/png/` 与 `/fonts/`，不会自动部署相册目录。接入本模块时必须在该函数中创建 `${LITTLEFS_STAGE_DIR}/photo`，并只复制 `app/ui/home/sub_home/album/photo/*.png`；无合格 PNG 时生成空 `/photo/` 或在构建阶段明确失败，策略必须固定且记录日志。不得把现有非 PNG 文件悄悄转换、混入或打包。

### 1.2 模块职责

| 职责 | `album_flash` | 上层 UI |
| --- | --- | --- |
| 枚举 `/photo/` | 负责 | 禁止 |
| 过滤 PNG、排序和索引映射 | 负责 | 禁止 |
| 打开文件、读取数据、记录长度 | 负责 | 禁止 |
| 输出图片数组 | 负责 | 只读使用 |
| 创建 `lv_img`、手势与页码 | 禁止 | `album` 负责 |
| 首页进入与返回手势 | 禁止 | `home_ui`、`home_album` 负责 |
| SD 卡或其他路径 | 禁止 | 禁止 |

## 2. 对外数据结构与 API 契约

`album_flash.h` 应定义统一的当前图片描述符。描述符与其中的图片字节数组由 `album_flash` 所有，上层只读使用，不得修改字段、释放 `data` 或缓存底层文件句柄。

```c
typedef struct {
    const uint8_t *data;
    size_t data_len;
    size_t index;
} album_flash_image_t;

bool album_flash_init(void);
size_t album_flash_get_image_count(void);
const album_flash_image_t *album_flash_get_current_image(void);
bool album_flash_get_image_by_index(size_t index,
                                    const uint8_t **out_data,
                                    size_t *out_data_len);
void album_flash_deinit(void);
```

### 2.1 初始化与数量接口

| 接口 | 入参 | 返回 | 约束 |
| --- | --- | --- | --- |
| `album_flash_init()` | 无 | 成功返回 `true` | 每次进入完整相册页调用；先释放上一次状态，再扫描 `/photo/`、过滤 PNG、排序、写入总数；总数大于 0 时默认读取索引 `0` 为当前图片 |
| `album_flash_get_image_count()` | 无 | 合格图片总数量 | 返回初始化时遍历得到的总数，不触发重复扫描；未初始化、目录不存在或错误时返回 `0` 并记录日志 |
| `album_flash_get_current_image()` | 无 | 当前 `album_flash_image_t` 指针；无图片或错误时返回 `NULL` | 返回当前图片字节数组、长度和索引；不执行遍历 |
| `album_flash_deinit()` | 无 | 无 | 退出完整相册页时调用；释放当前图片数据和任何模块内部堆内存，并将总数、索引和指针清零 |

索引顺序必须稳定：以打包到 `/photo/` 的 PNG 文件名按固定字典序排序后，从 `0` 开始编号。不得把底层目录遍历返回的非确定顺序暴露给 UI；同一镜像的同一文件集合必须得到相同的索引顺序。

模块内部状态固定为 `s_image_count` 与 `s_current_image`。`s_image_count` 仅在 `album_flash_init()` 扫描目录时更新；`s_current_image` 保存当前图片的 `data`、`data_len` 和 `index`。为按索引读取而产生的临时排序或路径数据只能在 `album_flash.c` 内部存活，完成读取后立即释放，不得向上层暴露为常驻图片列表。

### 2.2 当前图片数组与索引读取接口

| 接口 | 入参 | 返回 | 约束 |
| --- | --- | --- | --- |
| `album_flash_get_current_image()` | 无 | 当前图片描述符；无图片返回 `NULL` | `data` 是可供上层显示的只读图片字节数组，生命周期到 `deinit()` |
| `album_flash_get_image_by_index()` | `index`、非空 `out_data`、非空 `out_data_len` | 索引有效返回 `true`，输出当前图片字节数组和准确长度 | 必须先释放模块的前一张图片数据，再加载并输出目标项；越界、未初始化或读取失败返回 `false` |

`data` 指向模块拥有的只读 PNG 数据，`data_len` 为该数据的准确字节数。`album_flash_init()` 在总数大于 0 时必须默认读取索引 `0`，使 `album_flash_get_current_image()` 可立即输出首张图片数据。具体实现可选择按需读取或缓存当前项，但必须保证：

1. 对已成功返回的当前项，`data` 与 `data_len` 在本次显示完成前保持配对有效；切换到下一项前，UI 必须先清空旧图像源，随后底层先释放旧数据、再加载新数据，整个过程不得同时保留两张图片字节数组。
2. 上层只能把当前图片字节数组和长度传给 `album` 显示逻辑，不能取得或传播通用文件路径。
3. 文件读失败、PNG 格式不支持或内存不足时，接口返回失败且记录原因；不得回退扫描 SD 卡、图标目录或任意绝对路径。

## 3. 与上层页面的接口关系

```text
home_ui --左滑--> home_album
home_album --album_flash_init()--> album_flash --LittleFS /photo/*.png
home_album --album_create(data, len, count)--> album
album --album_flash_get_image_by_index()--> 当前图片字节数组 --> P01 / I01
```

`home_album` 在 A01 被点击时取得数组与数量，然后调用：

```c
size_t image_count;
const album_flash_image_t *current;

album_flash_init();
image_count = album_flash_get_image_count();
current = album_flash_get_current_image();
album_create(current != NULL ? current->data : NULL,
             current != NULL ? current->data_len : 0,
             image_count);
```

该代码片段只表达接口调用顺序，不授权上层访问文件路径。`album.c` 的图片显示/切换函数必须接收 `const uint8_t *data`、`data_len`、`image_count` 和目标 `index`；`data` 只能由 `album_flash_get_current_image()` 或 `album_flash_get_image_by_index()` 输出。`album.c` 不能调用目录 API、`fopen()`、LittleFS 挂载 API 或 SD 卡 API。

## 4. 扩展约束

后续所有图片相关功能必须在 `album_flash.*` 内新增实现，包括但不限于：PNG 校验、元数据提取、缩略图索引、预加载、当前项缓存、解码适配、内存回收、排序策略和资源版本校验。新增能力必须保持总数接口、当前图片数据接口和图片字节数组入参向后兼容；`home_album.c` 和 `album.c` 不因底层存储策略变化而改动。

禁止在 UI 页面中新增以下逻辑：

1. 目录遍历、文件名排序、按索引拼接路径。
2. 图片格式判断、字节长度读取、缓存释放或 Flash 错误恢复。
3. SD 卡检测、挂载、路径回退或从网络、图标目录读取相册图片。
4. 为演示效果硬编码图片数据、文件名或示例图片。

## 5. 内存与页面生命周期约束

`album_flash.c` 中所有需要堆内存的接口调用必须包含 `components/platform/platform.h`，并只使用 `ps_malloc()`、`ps_calloc()` 或 `ps_realloc()` 申请 PSRAM 内存；释放与这些接口配对的内存时使用标准 `free()`。禁止直接调用 `malloc()`、`calloc()` 或 `realloc()`，也禁止把堆内存的所有权转交给 UI 层。

1. `album_flash_init()` 先执行与 `deinit()` 等价的内部清理，防止重复进入相册泄漏上一轮当前图片数据。
2. 读取新索引时，必须先 `free(s_current_image.data)` 并清零 `data`、`data_len`、`index`，再申请和读取新数据；任一加载步骤失败时保持空状态并报告错误，不能保留旧图片、留下悬空指针或同时加载两张图片。
3. `album` 退出时，必须先清理 P01 的图像源和页面对象，再调用 `album_flash_deinit()`；这样 LVGL 不会在下一帧访问已释放的图片字节数组。
4. `album_flash_deinit()` 必须 `free(s_current_image.data)`，释放所有临时/缓存堆内存，并将 `s_image_count = 0`、`s_current_image.data = NULL`、`data_len = 0`、`index = 0`。

## 6. 构建、集成与验证

1. 在 `app/CMakeLists.txt` 的 `SRCS` 中注册 `ui/home/sub_home/album/album_flash.c`，并将 `ui/home/sub_home/album` 加入 `INCLUDE_DIRS`；`album.c` 和 `home_album.c` 同时按相同方式注册。
2. 扩展 `build.sh` 的 LittleFS staging：保留现有 `/png/` 和 `/fonts/` 行为，新增 `/photo/`，只复制源目录中的 PNG；随后由同一 `mklittlefs` 命令生成 `build/littlefs_icons.bin`。镜像文件名沿用既有构建输出，不代表其内容仅包含图标。
3. 调用 `album_flash_init()` 前必须完成 LittleFS 挂载；项目当前挂载点为 `/littlefs`，LVGL 文件系统盘符为 `R:`。
4. 运行 `./build.sh build` 验证代码；运行 `./build.sh build flash` 验证 LittleFS 镜像中存在 `/photo/` 且仅含 PNG。生成镜像与真实设备烧录分开，未经明确授权不得执行 `./build.sh flash`。
5. 运行时验证至少包括：空目录时完整相册 P01 中央显示“空”、有图片时初始化默认索引 `0`、单张 PNG、多张 PNG、非 PNG 被拒绝、索引边界、文件读取失败、重复进入不泄漏，以及退出页面 `deinit()` 后当前图片数据不可再使用。最后执行 `git diff --check`。
