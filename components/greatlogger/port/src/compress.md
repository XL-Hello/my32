封包设计：

```text
LZ4_HEAD + LZ4_DATA
```

其中 `LZ4_HEAD` 采用 `__attribute__((packed))` 固定布局。按你列出的 1–7 字段，建议先定义为：

```c
typedef struct __attribute__((packed)) {
    uint32_t block_seq;         // 块序号/逻辑地址
    uint32_t block_count;       // 头部 + LZ4_DATA 占用的物理 block 总数
    uint64_t rtc_time;          // 压缩完成时的单调 RTC 时间
    uint32_t head_crc32;        // block_seq + block_count + rtc_time 的 CRC32
    uint32_t raw_data_crc32;    // 压缩前 RAW_DATA 的 CRC32
    uint32_t lz4_data_crc32;    // 压缩后 LZ4_DATA 的 CRC32
    uint32_t magic;             // 0x55AA66BB
} lz4_head_t;
```

该结构固定长度：

```text
n = sizeof(lz4_head_t) = 32 字节
```

封包关系为：

```text
RAW_DATA(len=x)
    └─ LZ4 compress ─> LZ4_DATA(len=y)

最终输出：
[LZ4_HEAD: 32 bytes][LZ4_DATA: y bytes]

output_size = n + y
```

各校验字段的计算范围应固定为：

```text
head_crc32     = CRC32(block_seq || block_count || rtc_time)
raw_data_crc32 = CRC32(RAW_DATA[0..x))
lz4_data_crc32 = CRC32(LZ4_DATA[0..y))
```

解压/读取时按以下顺序判断：

1. 读取固定 32 字节 `LZ4_HEAD`。
2. 验证 `magic == 0x55AA66BB`。
3. 验证 `head_crc32`；失败则头信息不可信，不应再使用 `block_count` 或 `rtc_time`。
4. 用 `block_count` 确定本包占用范围，并检查没有越过分区边界。
5. 读取 `LZ4_DATA`，先验证 `lz4_data_crc32`。
6. 对 LZ4 数据解压。
7. 验证解压结果的 `raw_data_crc32`。

`block_count` 的计算应定义为：

```text
block_count = ceil((sizeof(lz4_head_t) + lz4_data_len) / storage_block_size)
```

其中 `storage_block_size` 必须是日志存储层实际使用的物理 block 大小，例如 Flash 擦除块、页面或你定义的日志分配块；这个单位必须全系统统一。

RTC 单调规则：

```text
rtc_time = max(current_rtc_time, previous_rtc_time + 1)
```

其中 `previous_rtc_time` 是上一条已成功封包记录的时间。应在封包成功后才更新它，避免压缩失败却消耗时间序列。

但这里有两个必须补足的关键信息：你列出的 1–7 字段无法让数据包完全独立解压。

- 缺少 `raw_data_len`：`LZ4_decompress_safe()` 必须知道目标输出缓冲区容量；仅有 CRC 无法推导原始长度。
- 缺少 `lz4_data_len` 或 `packet_len`：`block_count` 只能给出向上取整后的物理占用空间，不能得出最后一个 block 内实际 LZ4 数据的结束位置。LZ4 raw block 本身也没有可靠的结尾长度信息。

因此，如果要求“从 Flash 任意位置读到一个 `LZ4_HEAD` 后可独立提取与解压”，建议在头内增加两个字段，并将它们纳入 `head_crc32`：

```c
uint32_t raw_data_len;   // x，解压缓冲区准确容量
uint32_t lz4_data_len;   // y，LZ4 输入准确长度
```

此时推荐的头长度为 **40 字节**，且：

```text
block_count = ceil((40 + lz4_data_len) / storage_block_size)
```

若严格只保留现有 1–7 字段，则必须由存储索引或其他包外元数据可靠提供 `raw_data_len` 与 `lz4_data_len`；否则不能安全、确定地完成 LZ4 raw block 解压。

仅修改 `compress.c` 时，压缩封包和 CRC 校验可以实现，`glog_lz4_bound()` 也可自动加上头长；但 `block_seq`、实际存储 block 大小、跨重启的上一条 `rtc_time` 都属于存储分配层状态，`compress.c` 本身无法可靠获知。若强行在其中维护静态序号/时间，只能保证本次运行期间单调，重启后会失效。

术语上，`__attribute__((packed))` 属于内存布局控制：它去掉编译器自动插入的填充字节，让头部固定为预期长度；实际解析时仍建议按字节读写字段，避免未对齐访问和字节序差异。