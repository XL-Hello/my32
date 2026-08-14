idf_component_register(
    SRCS
        "glog.c"
        "glog_test.c"
        "./port/src/glog_crc32.c"
        "./port/src/compress.c"
        "./port/src/mempool.c"
        "./port/src/platform.c"
        "./port/src/queue.c"
        "./port/src/ringbuffer.c"
        "./port/3rd/lwmem/lwmem/src/lwmem/lwmem.c"
        "./port/3rd/lz4/lib/lz4.c"
    INCLUDE_DIRS
        "."
    PRIV_INCLUDE_DIRS
        "./port/inc"
        "./port/3rd/lwmem"
        "./port/3rd/lwmem/lwmem/src/include"
        "./port/3rd/lz4/lib"
    PRIV_REQUIRES freertos esp_hw_support platform platform_log
)

# greatlogger 只使用无 workspace 的 LZ4 block API，不编译会触达 libc heap 的
# stream create/free 便捷接口。
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION=1
)
