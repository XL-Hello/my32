# 仓库指南

## 项目结构与模块组织

本仓库是一个独立的 ESP-IDF `hello_world` 应用程序。

- `main/hello_world_main.c` 包含固件入口函数 `app_main()`。
- `main/CMakeLists.txt` 用于注册应用组件及其源文件。
- 根目录的 `CMakeLists.txt` 将 ESP-IDF 项目定义为 `hello_world`。
- `pytest_hello_world.py` 包含真机和 QEMU 集成测试。
- `sdkconfig.ci` 提供 CI 专用配置；本地 `sdkconfig` 是生成的配置文件，除非确有必要，否则不要手动编辑。
- `.devcontainer/` 和 `.vscode/` 保存项目提供的开发环境配置。

新增固件模块应放在 `main/` 下；当功能需要跨应用复用或规模较大时，在 `components/<名称>/` 下创建独立组件。

## 构建、测试与开发命令

本项目必须使用仓库内的 `esp-idf/`，不得使用 `/home/xl/.espressif/...` 下的全局 ESP-IDF。AI 执行构建、烧录、清理或测试前，必须通过项目脚本启用本地 SDK；推荐直接运行 `./build.sh build`。若需要直接运行 `idf.py`，必须先执行 `source ./build.sh env`，并确认 `IDF_PATH` 为 `${PWD}/esp-idf`。切换 SDK 后由 `build.sh` 负责清理旧构建缓存，禁止直接加载全局 SDK 的 `export.sh`。

使用项目内 ESP-IDF 的命令如下：

```bash
./build.sh build           # 使用项目内 esp-idf/ 配置并编译到 build/ 目录
source ./build.sh env       # 为当前终端启用项目内 ESP-IDF，之后才可直接调用 idf.py
idf.py set-target esp32s3   # 选择目标芯片，可替换为其他受支持的目标
idf.py build               # 已执行 source ./build.sh env 后可用
idf.py -p /dev/ttyUSB0 flash monitor  # 烧录固件并打开串口监视器
idf.py fullclean            # 删除生成的构建状态
pytest -v pytest_hello_world.py --target esp32s3  # 运行 pytest-embedded 测试
```

真机测试会等待 `Hello world!` 输出并验证堆内存日志。QEMU 测试目前仅标记为支持 ESP32。

## 编码风格与命名规范

遵循 `main/hello_world_main.c` 中已有的 ESP-IDF C 风格：使用四个空格缩进，控制语句的左花括号与语句同行，标识符采用 `snake_case`，配置宏采用 `UPPER_SNAKE_CASE`。使用定宽整数类型及匹配的格式宏，例如 `PRIu32`。在 CMake 中明确列出组件源文件。Python 测试使用四个空格缩进，适当添加类型注解，测试名采用 `snake_case`，并用 pytest 标记注明支持的目标。

## 文档语言规范

本项目的文档、开发记录、提交说明和面向贡献者的操作指引默认使用中文。代码标识符、命令、路径、配置项及无法准确翻译的技术术语保留原文。新增或修改 Markdown 文档时，应使用简洁、明确的中文，并确保示例命令可以直接执行。

## 测试规范

Python 测试文件命名为 `pytest_<功能>.py`，测试函数命名为 `test_<行为>`。为可观察的固件行为添加串口输出断言，并明确标记目标限制。提交前，应针对受影响的目标运行 `idf.py build` 和相关 pytest 命令。当前项目未配置测试覆盖率阈值。

## 提交与合并请求规范

当前提交历史较少，提交说明采用简短、直接的描述。每个提交应聚焦单一变更并说明结果，例如 `添加启动日志测试`。合并请求应注明目标芯片、概述行为变化、列出已运行的构建和测试命令，并附上相关串口输出。关联对应问题；仅在涉及界面或工具变更时附加截图。
