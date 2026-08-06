# 本地固件与资源 HTTP 服务

该工具在开发电脑上提供网页管理台和 HTTP 下载接口，供 ESP32 拉取 OTA 固件或资源。

## 双击启动

双击 `launch.py`。首次启动会自动从 `config.example.json` 创建 `config.json`，启动网页服务并打开浏览器。关闭该网页后服务会在 5 秒内自动退出。

也可以在终端中手动运行：

```bash
cd tools/local_ftp
./start.sh
```

浏览器打开 `http://<电脑局域网IP>:8080`。ESP32 使用同一地址的 `/api/manifest` 和 `/files/<路径>` 接口下载文件。

## 文件约定

```text
storage/
├── manifest.json
├── ota/
│   └── hello_world.bin
└── resources/
```

网页上传“OTA 固件”后会自动写入 `storage/ota/` 并更新 `manifest.json`，其中包含文件路径、大小、SHA-256 与发布时间。ESP32 应先下载该清单，校验完成后再下载目标文件。

网页上传采用临时文件后原子替换，避免上传未完成的文件被客户端读取。当前 HTTP 服务适合受控局域网开发环境；不要直接暴露到公网。

## ESP32 接入建议

1. 在项目 `menuconfig` 中配置电脑的 HTTP 服务地址。
2. 手动点击系统信息后才检查 `manifest.json`，不要在启动时自动升级。
3. 下载 OTA 时写入未运行 OTA 分区，复用现有 SHA-256、ESP 镜像校验和回滚流程。
4. 下载资源时先写临时文件，校验 SHA-256 后再替换正式资源。

`server.py` 仅使用 Python 标准库，无需安装第三方依赖。
