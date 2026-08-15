# Windows 搭建 EMQX MQTT Broker

本文用于在 Windows 本机启动 EMQX 5.3.2，供 ESP32 或 MQTTX 等客户端连接测试。

> EMQX 当前版本的官方支持平台不含 Windows。本教程适用于已有的 Windows 安装包或本地开发环境；新项目建议在 Linux、Docker 或 EMQX Cloud 上部署。

## 1. 安装与启动

1. 下载并解压 `emqx-5.3.2-windows-amd64.zip`。安装路径不要包含中文、空格或特殊字符，例如 `D:\emqx`。
2. 以管理员身份打开 PowerShell 或 CMD，并进入 `bin` 目录：

   ```powershell
   cd D:\emqx\bin
   ```

3. 首次安装为 Windows 服务：

   ```powershell
   .\emqx.cmd install
   ```

4. 前台启动并查看日志：

   ```powershell
   .\emqx.cmd console
   ```

   出现 `is running now!` 表示启动成功。停止前台服务时按 `Ctrl+C`。

常用命令：

```powershell
.\emqx.cmd start       # 后台启动
.\emqx.cmd stop        # 停止
.\emqx.cmd uninstall   # 卸载 Windows 服务
```

## 2. 登录控制台

浏览器打开 `http://localhost:18083`，首次登录使用：

- 用户名：`admin`
- 密码：`public`

登录后立即修改初始密码。

## 3. 用 MQTTX 验证

在 MQTTX 创建连接，填写：

| 配置项 | 值 |
| --- | --- |
| Host | `127.0.0.1` |
| Port | `1883` |
| 协议 | MQTT |

连接成功后，在客户端 A 订阅主题 `test/hello`，在客户端 B 向同一主题发布任意消息；A 能收到消息即表示 Broker 工作正常。

设备连接电脑上的 Broker 时，将 Host 改为电脑的局域网 IP，并在 Windows 防火墙中放行 TCP 端口 `1883`；管理控制台端口为 `18083`。

## 4. 常见问题

- 启动提示 `Unable to load emulator DLL`：安装与当前 EMQX 安装包匹配的 Erlang/OTP，重新打开终端后再启动。
- 无法访问控制台：确认 EMQX 已运行，并检查端口 `18083` 是否被占用。
- 客户端无法连接：确认地址、端口和防火墙配置；本机测试优先使用 `127.0.0.1:1883`。
