#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "littlefs_test.h"

#define LOG_TAG "littlefs"
#include "platform_log.h"

#define LITTLEFS_TEST_PATH        "/littlefs/test.txt"
#define LITTLEFS_SPEED_TEST_PATH  "/littlefs/speed_test.bin"
#define LITTLEFS_SPEED_TEST_BYTES (512U * 1024U)
#define LITTLEFS_SPEED_TEST_CHUNK 4096U

static void littlefs_log_speed(const char *operation, size_t bytes, int64_t elapsed_us)
{
    if (elapsed_us <= 0) {
        elapsed_us = 1;
    }

    uint64_t kib_per_second = (uint64_t)bytes * 1000000ULL / ((uint64_t)elapsed_us * 1024ULL);
    log_info("%s 512 KiB: %" PRId64 " us, %" PRIu64 " KiB/s",
             operation, elapsed_us, kib_per_second);
}

esp_err_t littlefs_esp_test(void)
{
    static const char write_data[] = "HELLO LittleFS";
    char read_data[sizeof(write_data)] = { 0 };

    FILE *file = fopen(LITTLEFS_TEST_PATH, "w");
    if (file == NULL) {
        log_error("create test file failed: %s", strerror(errno));
        return ESP_FAIL;
    }

    size_t written = fwrite(write_data, 1, sizeof(write_data) - 1, file);
    if (written != sizeof(write_data) - 1 || fflush(file) != 0 || fclose(file) != 0) {
        log_error("save test file failed: %s", strerror(errno));
        return ESP_FAIL;
    }

    file = fopen(LITTLEFS_TEST_PATH, "r");
    if (file == NULL) {
        log_error("open saved test file failed: %s", strerror(errno));
        return ESP_FAIL;
    }

    size_t read = fread(read_data, 1, sizeof(read_data) - 1, file);
    bool read_failed = ferror(file) != 0;
    if (fclose(file) != 0 || read_failed) {
        log_error("read test file failed: %s", strerror(errno));
        return ESP_FAIL;
    }
    if (read != sizeof(write_data) - 1 || memcmp(read_data, write_data, read) != 0) {
        log_error("test file data verification failed");
        return ESP_FAIL;
    }
    if (remove(LITTLEFS_TEST_PATH) != 0) {
        log_error("delete test file failed: %s", strerror(errno));
        return ESP_FAIL;
    }

    log_info("read test file: %s", read_data);
    return ESP_OK;
}

esp_err_t littlefs_esp_speed_test(void)
{
    static uint8_t transfer_buffer[LITTLEFS_SPEED_TEST_CHUNK];
    size_t transferred = 0;
    memset(transfer_buffer, 0xA5, sizeof(transfer_buffer));

    int64_t start_us = esp_timer_get_time();
    FILE *file = fopen(LITTLEFS_SPEED_TEST_PATH, "w");
    if (file == NULL) {
        log_error("open speed-test file for write failed: %s", strerror(errno));
        return ESP_FAIL;
    }

    while (transferred < LITTLEFS_SPEED_TEST_BYTES) {
        size_t remaining = LITTLEFS_SPEED_TEST_BYTES - transferred;
        size_t chunk_size = remaining < sizeof(transfer_buffer) ? remaining : sizeof(transfer_buffer);
        if (fwrite(transfer_buffer, 1, chunk_size, file) != chunk_size) {
            log_error("write speed-test file failed: %s", strerror(errno));
            fclose(file);
            return ESP_FAIL;
        }
        transferred += chunk_size;
    }
    if (fclose(file) != 0) {
        log_error("close written speed-test file failed: %s", strerror(errno));
        return ESP_FAIL;
    }
    littlefs_log_speed("write", transferred, esp_timer_get_time() - start_us);

    transferred = 0;
    start_us = esp_timer_get_time();
    file = fopen(LITTLEFS_SPEED_TEST_PATH, "r");
    if (file == NULL) {
        log_error("open speed-test file for read failed: %s", strerror(errno));
        return ESP_FAIL;
    }

    while (transferred < LITTLEFS_SPEED_TEST_BYTES) {
        size_t remaining = LITTLEFS_SPEED_TEST_BYTES - transferred;
        size_t chunk_size = remaining < sizeof(transfer_buffer) ? remaining : sizeof(transfer_buffer);
        size_t read = fread(transfer_buffer, 1, chunk_size, file);
        if (read != chunk_size) {
            bool read_failed = ferror(file) != 0;
            fclose(file);
            log_error("read speed-test file failed: %s", read_failed ? strerror(errno) : "unexpected EOF");
            return ESP_FAIL;
        }
        transferred += read;
    }
    if (fclose(file) != 0) {
        log_error("close read speed-test file failed: %s", strerror(errno));
        return ESP_FAIL;
    }
    littlefs_log_speed("read", transferred, esp_timer_get_time() - start_us);
    if (remove(LITTLEFS_SPEED_TEST_PATH) != 0) {
        log_error("delete speed-test file failed: %s", strerror(errno));
        return ESP_FAIL;
    }
    return ESP_OK;
}
