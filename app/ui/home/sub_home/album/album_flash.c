#include "album_flash.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "platform.h"

#define LOG_TAG "album_flash"
#include "platform_log.h"

#define ALBUM_FLASH_LITTLEFS_ROOT "/littlefs"
#define ALBUM_FLASH_PHOTO_DIR ALBUM_FLASH_LITTLEFS_ROOT "/photo"
#define ALBUM_FLASH_PATH_SIZE 256
#define ALBUM_FLASH_PNG_SIGNATURE_SIZE 8

static const uint8_t s_png_signature[ALBUM_FLASH_PNG_SIGNATURE_SIZE] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
};

static bool s_initialized;
static size_t s_image_count;
static album_flash_image_t s_current_image;

static void album_flash_log_directory(const char *directory_path)
{
    DIR *directory = opendir(directory_path);
    if (directory == NULL) {
        log_error("无法列出 LittleFS 目录：%s，错误=%d", directory_path, errno);
        return;
    }

    log_info("LittleFS 根目录一级内容：%s", directory_path);
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (entry->d_type == DT_DIR) {
            log_info("文件夹：%s", entry->d_name);
        } else if (entry->d_type == DT_REG) {
            log_info("文件：%s", entry->d_name);
        } else {
            log_info("其他条目：%s", entry->d_name);
        }
    }

    closedir(directory);
}

static void album_flash_clear_current_image(void)
{
    free((void *)s_current_image.data);
    s_current_image.data = NULL;
    s_current_image.data_len = 0;
    s_current_image.index = 0;
}

static bool album_flash_is_png_name(const char *name)
{
    const char *extension = strrchr(name, '.');
    return extension != NULL && strcmp(extension, ".png") == 0;
}

static int album_flash_compare_names(const void *left, const void *right)
{
    const char *const *left_name = left;
    const char *const *right_name = right;
    return strcmp(*left_name, *right_name);
}

static void album_flash_free_names(char **names, size_t count)
{
    if (names == NULL) {
        return;
    }

    for (size_t index = 0; index < count; ++index) {
        free(names[index]);
    }
    free(names);
}

static bool album_flash_collect_names(char ***out_names, size_t *out_count)
{
    if (out_names == NULL || out_count == NULL) {
        return false;
    }

    *out_names = NULL;
    *out_count = 0;

    DIR *directory = opendir(ALBUM_FLASH_PHOTO_DIR);
    if (directory == NULL) {
        if (errno == ENOENT) {
            /* mklittlefs 会省略空目录；缺少 /photo 等价于空相册。 */
            log_info("相册目录不存在，按空相册处理：%s", ALBUM_FLASH_PHOTO_DIR);
            return true;
        }
        log_error("无法打开相册目录：%s，错误=%d", ALBUM_FLASH_PHOTO_DIR, errno);
        return false;
    }

    char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;
    bool success = true;
    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL) {
        if (!album_flash_is_png_name(entry->d_name)) {
            continue;
        }

        char path[ALBUM_FLASH_PATH_SIZE];
        const int path_length = snprintf(path, sizeof(path), "%s/%s",
                                         ALBUM_FLASH_PHOTO_DIR, entry->d_name);
        if (path_length < 0 || (size_t)path_length >= sizeof(path)) {
            log_error("相册文件名过长，已忽略：%s", entry->d_name);
            continue;
        }

        struct stat file_status;
        if (stat(path, &file_status) != 0 || !S_ISREG(file_status.st_mode)) {
            log_error("相册条目不是可读取 PNG：%s", entry->d_name);
            continue;
        }

        if (count == capacity) {
            const size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
            if (new_capacity <= capacity || new_capacity > SIZE_MAX / sizeof(*names)) {
                log_error("相册文件数量超出可分配范围");
                success = false;
                break;
            }

            char **new_names = ps_realloc(names, new_capacity * sizeof(*names));
            if (new_names == NULL) {
                log_error("相册文件名索引分配失败");
                success = false;
                break;
            }
            names = new_names;
            capacity = new_capacity;
        }

        const size_t name_size = strlen(entry->d_name) + 1;
        names[count] = ps_malloc(name_size);
        if (names[count] == NULL) {
            log_error("相册文件名分配失败：%s", entry->d_name);
            success = false;
            break;
        }
        memcpy(names[count], entry->d_name, name_size);
        ++count;
    }

    closedir(directory);
    if (!success) {
        album_flash_free_names(names, count);
        return false;
    }

    qsort(names, count, sizeof(*names), album_flash_compare_names);
    *out_names = names;
    *out_count = count;
    return true;
}

static bool album_flash_load_name(const char *name, size_t index)
{
    char path[ALBUM_FLASH_PATH_SIZE];
    const int path_length = snprintf(path, sizeof(path), "%s/%s", ALBUM_FLASH_PHOTO_DIR, name);
    if (path_length < 0 || (size_t)path_length >= sizeof(path)) {
        log_error("相册文件名过长，无法读取：%s", name);
        return false;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        log_error("无法读取相册图片：%s，错误=%d", path, errno);
        return false;
    }

    bool success = false;
    uint8_t *data = NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        log_error("无法定位相册图片末尾：%s", path);
        goto cleanup;
    }

    const long file_size = ftell(file);
    if (file_size < ALBUM_FLASH_PNG_SIGNATURE_SIZE || (unsigned long)file_size > UINT32_MAX) {
        log_error("相册图片大小无效：%s", path);
        goto cleanup;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        log_error("无法定位相册图片开头：%s", path);
        goto cleanup;
    }

    const size_t data_len = (size_t)file_size;
    data = ps_malloc(data_len);
    if (data == NULL) {
        log_error("相册图片 PSRAM 分配失败：%s", path);
        goto cleanup;
    }

    if (fread(data, 1, data_len, file) != data_len) {
        log_error("相册图片读取不完整：%s", path);
        goto cleanup;
    }

    if (memcmp(data, s_png_signature, ALBUM_FLASH_PNG_SIGNATURE_SIZE) != 0) {
        log_error("相册文件不是 PNG：%s", path);
        goto cleanup;
    }

    s_current_image.data = data;
    s_current_image.data_len = data_len;
    s_current_image.index = index;
    data = NULL;
    success = true;

cleanup:
    free(data);
    fclose(file);
    return success;
}

bool album_flash_init(void)
{
    album_flash_deinit();

    log_info("开始列出 LittleFS 文件");
    album_flash_log_directory(ALBUM_FLASH_LITTLEFS_ROOT);
    log_info("LittleFS 文件列表结束");

    char **names = NULL;
    size_t count = 0;
    if (!album_flash_collect_names(&names, &count)) {
        return false;
    }

    album_flash_free_names(names, count);
    s_initialized = true;
    s_image_count = count;
    if (count == 0) {
        log_info("相册目录中没有 PNG 图片");
        return true;
    }

    const uint8_t *data = NULL;
    size_t data_len = 0;
    if (!album_flash_get_image_by_index(0, &data, &data_len)) {
        log_error("无法加载默认相册图片");
        return false;
    }

    log_info("已加载相册索引，PNG 数量=%u", (unsigned int)s_image_count);
    return true;
}

size_t album_flash_get_image_count(void)
{
    if (!s_initialized) {
        log_error("相册资源尚未初始化");
        return 0;
    }
    return s_image_count;
}

const album_flash_image_t *album_flash_get_current_image(void)
{
    if (!s_initialized || s_current_image.data == NULL || s_current_image.data_len == 0) {
        return NULL;
    }
    return &s_current_image;
}

bool album_flash_get_image_by_index(size_t index, const uint8_t **out_data,
                                    size_t *out_data_len)
{
    if (out_data == NULL || out_data_len == NULL) {
        log_error("相册图片输出参数为空");
        return false;
    }
    *out_data = NULL;
    *out_data_len = 0;

    if (!s_initialized || index >= s_image_count) {
        log_error("相册图片索引无效：%u", (unsigned int)index);
        return false;
    }

    album_flash_clear_current_image();

    char **names = NULL;
    size_t count = 0;
    if (!album_flash_collect_names(&names, &count)) {
        return false;
    }

    if (count != s_image_count) {
        log_error("相册目录在页面显示期间发生变化");
        album_flash_free_names(names, count);
        return false;
    }

    const bool success = album_flash_load_name(names[index], index);
    album_flash_free_names(names, count);
    if (!success) {
        return false;
    }

    *out_data = s_current_image.data;
    *out_data_len = s_current_image.data_len;
    return true;
}

void album_flash_deinit(void)
{
    album_flash_clear_current_image();
    s_image_count = 0;
    s_initialized = false;
}
