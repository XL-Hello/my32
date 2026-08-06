# 本地 OTA 升级方案（面试总结）

## 1. 方案概览

本项目采用 ESP-IDF A/B OTA：控制中心的“系统信息”被点击后，后台通过局域网 HTTP 服务先读取 `/api/manifest`，再下载当前发布的 OTA 包并写入**未运行**的应用槽。写入及校验完成后设置下一启动槽并重启。

核心目标是任何升级中断或校验失败都不破坏当前可运行固件；新固件首次启动未通过自检时，自动回退到上一个已确认版本。

点击“系统信息”后，界面会显示转圈动画与“正在升级中”。该全屏遮罩会阻止其他操作，并持续显示到设备重启；若 HTTP 下载、校验或写入失败，动画停止并提示点击屏幕返回控制中心。

为避免大文件校验和写入阻塞 LVGL，升级任务每处理 16 KiB 数据会主动让出 CPU，目标延时为 2 ms。当前 `CONFIG_FREERTOS_HZ=100` 的实际调度粒度为 10 ms，因此实现会至少让出一个 tick（约 10 ms）；若需要严格 2 ms 调度，需评估后将全局 FreeRTOS tick 调整为 1 kHz。

## 2. 分区设计与容量

| 分区 | 偏移 | 大小 | 职责 |
| --- | ---: | ---: | --- |
| `otadata` | `0x10000` | 8 KiB | 两份 OTA 选择记录，决定下次启动槽与镜像状态 |
| `ota_0` | `0x20000` | 2 MiB | 应用槽 A |
| `ota_1` | `0x220000` | 2 MiB | 应用槽 B |
| 未分区 | `0x420000` | 1920 KiB | 预留备用空间，不被程序擦写 |
| `littlefs` | `0x600000` | 2 MiB | UI 资源与本地升级包 |

两个 app 槽大小必须相同，且固件二进制必须不大于 2 MiB。LittleFS 固定占最后 2 MiB，升级文件与 UI 资源共同占用该空间，应在发布前检查其剩余容量。

## 3. 正常升级时序

```text
点击“系统信息”
  → GET /api/manifest
  → 校验清单中的文件路径、大小与 SHA-256
  → GET /files/<OTA 文件路径>
  → esp_ota_begin() 擦除备用槽
  → esp_ota_write() 顺序写入
  → esp_ota_end() 校验目标镜像
  → esp_image_verify() 再次完整复检
  → esp_ota_set_boot_partition()
  → esp_restart()
  → Bootloader 将新槽标为 PENDING_VERIFY 后启动
  → 新固件启动自检通过
  → esp_ota_mark_app_valid_cancel_rollback()
```

回滚由 `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` 启用。新镜像在 `PENDING_VERIFY` 状态只允许试运行一次；如果首次启动时掉电、看门狗复位、崩溃，或应用主动标记无效，Bootloader 下次启动将自动回到上一个有效镜像。

当前固件把“关键服务已经启动”作为确认点：LittleFS 已挂载、Wi-Fi 管理、传感器、LVGL、LED 初始化均已完成后，调用 `local_ota_confirm_running_app()` 确认。若产品还有云登录、外设通讯或业务自检要求，应将确认点延后到这些关键检查成功之后；不能过早确认，否则业务层故障无法自动回滚。

## 4. 固件包制作与首次部署

首次从旧 `factory` 分区切换到本方案会重排 Flash，必须完整烧录：

```bash
./build.sh build
./build.sh flash /dev/ttyACM0
```

启动网页服务并上传 OTA 包：

```bash
cd tools/local_ftp
cp config.example.json config.json
./start.sh
```

浏览器打开 `http://<电脑IP>:8080`，上传 `build/hello_world.bin` 并选择“OTA 固件”。网页会发布 `manifest.json` 和下载路径。然后在项目 `menuconfig` 的“本地 OTA”中配置 `http://<电脑IP>:8080`，设备点击“系统信息”才会检查并下载升级包。

## 5. 异常场景、结果与处理

| 异常 | 代码/Bootloader 行为 | 设备结果 | 面试回答要点 |
| --- | --- | --- | --- |
| 升级包不存在、无法打开 | `stat`/`fopen` 失败，任务退出 | 保持当前固件 | 文件系统错误不能影响启动槽 |
| 包不是 ESP 镜像、头或段长度错误 | 写入前结构校验失败 | 不擦写备用槽 | 先校验后擦写，降低失败成本 |
| 文件传输损坏、SHA-256 不匹配 | 写入前内置哈希校验失败 | 不切换分区 | 完整性校验不等同来源可信 |
| 包超过 2 MiB | 与备用分区大小比较后拒绝 | 不写入 | A/B 槽大小决定最大固件尺寸 |
| 擦写或写 Flash 失败 | `esp_ota_begin`/`esp_ota_write` 失败并 `esp_ota_abort` | 当前槽不变 | 写入目标始终是非运行槽 |
| 写入完成但镜像损坏 | `esp_ota_end` 或显式 `esp_image_verify` 失败 | 不更新 `otadata` | 写后必须验证，不能只信传输过程 |
| 设置启动槽失败 | `esp_ota_set_boot_partition` 返回错误 | 仍启动旧版本 | 切换动作放在全部验证之后 |
| 切换后首次启动掉电、死机或 WDT | 新槽保持 `PENDING_VERIFY` | 下次由 Bootloader 回滚 | 回滚保护的是“首次试运行窗口” |
| 新固件自检失败 | 应调用 `esp_ota_mark_app_invalid_rollback_and_reboot()` | 立即回退 | 自检失败要主动回滚，不要仅打日志 |
| 自检通过后确认失败 | 当前实现请求标记无效并重启 | 回退到旧版本 | 宁可保守回退，不把未知状态标为有效 |
| LittleFS 空间不足或重建时断电 | 包可能不存在或哈希校验失败 | OTA 不开始 | LittleFS 本身不是升级原子存储，应有传输完成标记 |
| 重复点击升级 | `s_ota_running` 拒绝第二个任务 | 只有一个写任务 | 防止并发擦写同一 OTA 槽 |

## 6. 常见追问与边界

### 为什么还要写后再校验？

源文件校验只能证明读取时文件完整；Flash 擦写、供电波动、驱动错误或写入路径问题仍可能损坏目标镜像。`esp_ota_end()` 会做 ESP 镜像校验，本项目又显式调用 `esp_image_verify()` 复检，只有两者通过才更新 `otadata`。

### SHA-256 是否能防止恶意固件？

不能。镜像内置 SHA-256 仅用于发现意外损坏，攻击者可同时替换镜像与哈希。需要启用 Secure Boot、签名验签以及受保护的密钥；Flash Encryption 解决的是存储机密性，不替代签名认证。

### 回滚为什么不能确认得太早或太晚？

太早会把尚未完成业务自检的坏版本永久标记为有效；太晚会使正常启动期间的意外复位被视为失败并回退。应把确认点放在“设备对用户提供核心功能”的最小可用边界，并让该自检有明确超时。

### 还能继续升级一个待验证版本吗？

不能。ESP-IDF 在运行镜像仍为 `PENDING_VERIFY` 时拒绝下一次 OTA，以免丢失最后一个可回退版本。必须先确认当前版本有效，或让它回滚。

### 版本降级如何处理？

当前允许降级，以便回滚。若产品需要阻止人为刷回旧漏洞版本，可在验证该机制后再启用 anti-rollback 和 eFuse security version；这会与回滚策略产生约束，不能在不了解版本规划时贸然开启。

## 7. 发布与排查清单

1. 构建确认 `hello_world.bin` 小于 2 MiB，并保存其版本、SHA-256 与构建配置。
2. 检查 LittleFS 镜像包含 `ota/hello_world.bin`（LVGL：`R:/littlefs/ota/hello_world.bin`），且总容量不超过 2 MiB。
3. 首次部署后确认正在从 `ota_0` 启动；再执行一次升级，验证交替启动 `ota_1`。
4. 分别测试：缺文件、损坏包、超大包、升级中断电、首次启动时强制 WDT、自检失败。
5. 从串口记录 OTA 状态、目标分区、`esp_err_to_name()` 错误码和最终确认/回滚结果。

真机升级会改变 Flash 内容，测试时应保持已知可用的旧镜像和稳定供电；构建验证不能代替断电与回滚真机测试。
