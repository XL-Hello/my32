#include "littlefs_esp.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_partition.h"
#include "esp_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lfs.h"

#define LITTLEFS_MAX_OPEN_FILES 8
#define LITTLEFS_BASE_PATH_MAX  32
#define LITTLEFS_READ_SIZE      16
#define LITTLEFS_PROG_SIZE      16
#define LITTLEFS_CACHE_SIZE     256
#define LITTLEFS_LOOKAHEAD_SIZE 16

typedef struct {
    bool in_use;
    lfs_file_t file;
} littlefs_file_descriptor_t;

typedef struct {
    bool mounted;
    SemaphoreHandle_t mutex;
    const esp_partition_t *partition;
    lfs_t lfs;
    struct lfs_config config;
    char base_path[LITTLEFS_BASE_PATH_MAX];
    littlefs_file_descriptor_t files[LITTLEFS_MAX_OPEN_FILES];
} littlefs_context_t;

static littlefs_context_t s_context;

static int littlefs_error_to_errno(int error)
{
    switch (error) {
    case LFS_ERR_NOENT:
        return ENOENT;
    case LFS_ERR_EXIST:
        return EEXIST;
    case LFS_ERR_NOTDIR:
        return ENOTDIR;
    case LFS_ERR_ISDIR:
        return EISDIR;
    case LFS_ERR_NOTEMPTY:
        return ENOTEMPTY;
    case LFS_ERR_BADF:
        return EBADF;
    case LFS_ERR_FBIG:
        return EFBIG;
    case LFS_ERR_INVAL:
        return EINVAL;
    case LFS_ERR_NOSPC:
        return ENOSPC;
    case LFS_ERR_NOMEM:
        return ENOMEM;
    case LFS_ERR_NAMETOOLONG:
        return ENAMETOOLONG;
    default:
        return EIO;
    }
}

static void littlefs_set_errno(int error)
{
    errno = littlefs_error_to_errno(error);
}

static bool littlefs_lock(littlefs_context_t *context)
{
    if (xSemaphoreTake(context->mutex, portMAX_DELAY) == pdTRUE) {
        return true;
    }

    errno = EIO;
    return false;
}

static void littlefs_unlock(littlefs_context_t *context)
{
    xSemaphoreGive(context->mutex);
}

static bool littlefs_block_range_is_valid(const struct lfs_config *config,
                                          lfs_block_t block,
                                          lfs_off_t offset,
                                          lfs_size_t size)
{
    return block < config->block_count && offset <= config->block_size &&
           size <= config->block_size - offset;
}

static int littlefs_partition_read(const struct lfs_config *config,
                                   lfs_block_t block,
                                   lfs_off_t offset,
                                   void *buffer,
                                   lfs_size_t size)
{
    const esp_partition_t *partition = config->context;
    if (!littlefs_block_range_is_valid(config, block, offset, size)) {
        return LFS_ERR_IO;
    }

    size_t partition_offset = (size_t)block * config->block_size + offset;
    return esp_partition_read(partition, partition_offset, buffer, size) == ESP_OK ?
               LFS_ERR_OK :
               LFS_ERR_IO;
}

static int littlefs_partition_prog(const struct lfs_config *config,
                                   lfs_block_t block,
                                   lfs_off_t offset,
                                   const void *buffer,
                                   lfs_size_t size)
{
    const esp_partition_t *partition = config->context;
    if (!littlefs_block_range_is_valid(config, block, offset, size)) {
        return LFS_ERR_IO;
    }

    size_t partition_offset = (size_t)block * config->block_size + offset;
    return esp_partition_write(partition, partition_offset, buffer, size) == ESP_OK ?
               LFS_ERR_OK :
               LFS_ERR_IO;
}

static int littlefs_partition_erase(const struct lfs_config *config, lfs_block_t block)
{
    const esp_partition_t *partition = config->context;
    if (block >= config->block_count) {
        return LFS_ERR_IO;
    }

    size_t partition_offset = (size_t)block * config->block_size;
    return esp_partition_erase_range(partition, partition_offset, config->block_size) == ESP_OK ?
               LFS_ERR_OK :
               LFS_ERR_IO;
}

static int littlefs_partition_sync(const struct lfs_config *config)
{
    (void)config;
    return LFS_ERR_OK;
}

static const char *littlefs_path(const char *path)
{
    while (*path == '/') {
        ++path;
    }
    return path;
}

static int littlefs_open_flags(int flags)
{
    int lfs_flags;
    switch (flags & O_ACCMODE) {
    case O_WRONLY:
        lfs_flags = LFS_O_WRONLY;
        break;
    case O_RDWR:
        lfs_flags = LFS_O_RDWR;
        break;
    default:
        lfs_flags = LFS_O_RDONLY;
        break;
    }

    if ((flags & O_CREAT) != 0) {
        lfs_flags |= LFS_O_CREAT;
    }
    if ((flags & O_EXCL) != 0) {
        lfs_flags |= LFS_O_EXCL;
    }
    if ((flags & O_TRUNC) != 0) {
        lfs_flags |= LFS_O_TRUNC;
    }
    if ((flags & O_APPEND) != 0) {
        lfs_flags |= LFS_O_APPEND;
    }
    return lfs_flags;
}

static littlefs_file_descriptor_t *littlefs_get_file(littlefs_context_t *context, int fd)
{
    if (fd < 0 || fd >= LITTLEFS_MAX_OPEN_FILES || !context->files[fd].in_use) {
        errno = EBADF;
        return NULL;
    }
    return &context->files[fd];
}

static int littlefs_vfs_open(void *arg, const char *path, int flags, int mode)
{
    (void)mode;
    littlefs_context_t *context = arg;
    if (!littlefs_lock(context)) {
        return -1;
    }

    int fd;
    for (fd = 0; fd < LITTLEFS_MAX_OPEN_FILES; ++fd) {
        if (!context->files[fd].in_use) {
            break;
        }
    }
    if (fd == LITTLEFS_MAX_OPEN_FILES) {
        errno = EMFILE;
        littlefs_unlock(context);
        return -1;
    }

    int result = lfs_file_open(&context->lfs, &context->files[fd].file,
                               littlefs_path(path), littlefs_open_flags(flags));
    if (result != LFS_ERR_OK) {
        littlefs_set_errno(result);
        littlefs_unlock(context);
        return -1;
    }

    context->files[fd].in_use = true;
    littlefs_unlock(context);
    return fd;
}

static int littlefs_vfs_close(void *arg, int fd)
{
    littlefs_context_t *context = arg;
    if (!littlefs_lock(context)) {
        return -1;
    }

    littlefs_file_descriptor_t *file = littlefs_get_file(context, fd);
    if (file == NULL) {
        littlefs_unlock(context);
        return -1;
    }

    int result = lfs_file_close(&context->lfs, &file->file);
    file->in_use = false;
    if (result != LFS_ERR_OK) {
        littlefs_set_errno(result);
        littlefs_unlock(context);
        return -1;
    }

    littlefs_unlock(context);
    return 0;
}

static ssize_t littlefs_vfs_read(void *arg, int fd, void *buffer, size_t size)
{
    littlefs_context_t *context = arg;
    if (size > INT32_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (!littlefs_lock(context)) {
        return -1;
    }

    littlefs_file_descriptor_t *file = littlefs_get_file(context, fd);
    if (file == NULL) {
        littlefs_unlock(context);
        return -1;
    }

    lfs_ssize_t result = lfs_file_read(&context->lfs, &file->file, buffer, size);
    if (result < 0) {
        littlefs_set_errno(result);
        littlefs_unlock(context);
        return -1;
    }

    littlefs_unlock(context);
    return result;
}

static ssize_t littlefs_vfs_write(void *arg, int fd, const void *buffer, size_t size)
{
    littlefs_context_t *context = arg;
    if (size > INT32_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (!littlefs_lock(context)) {
        return -1;
    }

    littlefs_file_descriptor_t *file = littlefs_get_file(context, fd);
    if (file == NULL) {
        littlefs_unlock(context);
        return -1;
    }

    lfs_ssize_t result = lfs_file_write(&context->lfs, &file->file, buffer, size);
    if (result < 0) {
        littlefs_set_errno(result);
        littlefs_unlock(context);
        return -1;
    }

    littlefs_unlock(context);
    return result;
}

static off_t littlefs_vfs_lseek(void *arg, int fd, off_t offset, int whence)
{
    littlefs_context_t *context = arg;
    if (!littlefs_lock(context)) {
        return -1;
    }

    littlefs_file_descriptor_t *file = littlefs_get_file(context, fd);
    if (file == NULL) {
        littlefs_unlock(context);
        return -1;
    }

    int lfs_whence;
    switch (whence) {
    case SEEK_SET:
        lfs_whence = LFS_SEEK_SET;
        break;
    case SEEK_CUR:
        lfs_whence = LFS_SEEK_CUR;
        break;
    case SEEK_END:
        lfs_whence = LFS_SEEK_END;
        break;
    default:
        errno = EINVAL;
        littlefs_unlock(context);
        return -1;
    }

    lfs_soff_t result = lfs_file_seek(&context->lfs, &file->file, offset, lfs_whence);
    if (result < 0) {
        littlefs_set_errno(result);
        littlefs_unlock(context);
        return -1;
    }

    littlefs_unlock(context);
    return result;
}

static int littlefs_vfs_fsync(void *arg, int fd)
{
    littlefs_context_t *context = arg;
    if (!littlefs_lock(context)) {
        return -1;
    }

    littlefs_file_descriptor_t *file = littlefs_get_file(context, fd);
    if (file == NULL) {
        littlefs_unlock(context);
        return -1;
    }

    int result = lfs_file_sync(&context->lfs, &file->file);
    if (result != LFS_ERR_OK) {
        littlefs_set_errno(result);
        littlefs_unlock(context);
        return -1;
    }

    littlefs_unlock(context);
    return 0;
}

static int littlefs_vfs_fstat(void *arg, int fd, struct stat *stat_buffer)
{
    littlefs_context_t *context = arg;
    if (!littlefs_lock(context)) {
        return -1;
    }

    littlefs_file_descriptor_t *file = littlefs_get_file(context, fd);
    if (file == NULL) {
        littlefs_unlock(context);
        return -1;
    }

    lfs_soff_t size = lfs_file_size(&context->lfs, &file->file);
    if (size < 0) {
        littlefs_set_errno(size);
        littlefs_unlock(context);
        return -1;
    }

    memset(stat_buffer, 0, sizeof(*stat_buffer));
    stat_buffer->st_mode = S_IFREG | 0644;
    stat_buffer->st_size = size;
    littlefs_unlock(context);
    return 0;
}

static int littlefs_vfs_stat(void *arg, const char *path, struct stat *stat_buffer)
{
    littlefs_context_t *context = arg;
    if (!littlefs_lock(context)) {
        return -1;
    }

    struct lfs_info info;
    int result = lfs_stat(&context->lfs, littlefs_path(path), &info);
    if (result != LFS_ERR_OK) {
        littlefs_set_errno(result);
        littlefs_unlock(context);
        return -1;
    }

    memset(stat_buffer, 0, sizeof(*stat_buffer));
    stat_buffer->st_mode = (info.type == LFS_TYPE_DIR ? S_IFDIR : S_IFREG) | 0644;
    stat_buffer->st_size = info.size;
    littlefs_unlock(context);
    return 0;
}

static int littlefs_vfs_unlink(void *arg, const char *path)
{
    littlefs_context_t *context = arg;
    if (!littlefs_lock(context)) {
        return -1;
    }

    int result = lfs_remove(&context->lfs, littlefs_path(path));
    if (result != LFS_ERR_OK) {
        littlefs_set_errno(result);
        littlefs_unlock(context);
        return -1;
    }

    littlefs_unlock(context);
    return 0;
}

static const esp_vfs_t s_vfs = {
    .flags = ESP_VFS_FLAG_CONTEXT_PTR,
    .write_p = littlefs_vfs_write,
    .lseek_p = littlefs_vfs_lseek,
    .read_p = littlefs_vfs_read,
    .open_p = littlefs_vfs_open,
    .close_p = littlefs_vfs_close,
    .fstat_p = littlefs_vfs_fstat,
    .stat_p = littlefs_vfs_stat,
    .unlink_p = littlefs_vfs_unlink,
    .fsync_p = littlefs_vfs_fsync,
};

esp_err_t littlefs_esp_mount(const littlefs_esp_config_t *mount_config)
{
    if (mount_config == NULL || mount_config->base_path == NULL ||
        mount_config->partition_label == NULL || mount_config->base_path[0] != '/') {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_context.mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t base_path_length = strlen(mount_config->base_path);
    if (base_path_length == 0 || base_path_length >= sizeof(s_context.base_path)) {
        return ESP_ERR_INVALID_SIZE;
    }

    memset(&s_context, 0, sizeof(s_context));
    s_context.partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                   ESP_PARTITION_SUBTYPE_ANY,
                                                   mount_config->partition_label);
    if (s_context.partition == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    if (s_context.partition->erase_size == 0 ||
        s_context.partition->size % s_context.partition->erase_size != 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(s_context.base_path, mount_config->base_path, base_path_length + 1);
    s_context.mutex = xSemaphoreCreateMutex();
    if (s_context.mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_context.config = (struct lfs_config){
        .context = (void *)s_context.partition,
        .read = littlefs_partition_read,
        .prog = littlefs_partition_prog,
        .erase = littlefs_partition_erase,
        .sync = littlefs_partition_sync,
        .read_size = LITTLEFS_READ_SIZE,
        .prog_size = LITTLEFS_PROG_SIZE,
        .block_size = s_context.partition->erase_size,
        .block_count = s_context.partition->size / s_context.partition->erase_size,
        .block_cycles = 500,
        .cache_size = LITTLEFS_CACHE_SIZE,
        .lookahead_size = LITTLEFS_LOOKAHEAD_SIZE,
    };

    int result = lfs_mount(&s_context.lfs, &s_context.config);
    if (result == LFS_ERR_CORRUPT && mount_config->format_if_mount_failed) {
        result = lfs_format(&s_context.lfs, &s_context.config);
        if (result == LFS_ERR_OK) {
            result = lfs_mount(&s_context.lfs, &s_context.config);
        }
    }
    if (result != LFS_ERR_OK) {
        vSemaphoreDelete(s_context.mutex);
        memset(&s_context, 0, sizeof(s_context));
        return ESP_FAIL;
    }

    esp_err_t error = esp_vfs_register(s_context.base_path, &s_vfs, &s_context);
    if (error != ESP_OK) {
        lfs_unmount(&s_context.lfs);
        vSemaphoreDelete(s_context.mutex);
        memset(&s_context, 0, sizeof(s_context));
        return error;
    }

    s_context.mounted = true;
    return ESP_OK;
}

esp_err_t littlefs_esp_unmount(void)
{
    if (!s_context.mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t error = esp_vfs_unregister(s_context.base_path);
    if (error != ESP_OK) {
        return error;
    }

    int result = lfs_unmount(&s_context.lfs);
    vSemaphoreDelete(s_context.mutex);
    memset(&s_context, 0, sizeof(s_context));
    return result == LFS_ERR_OK ? ESP_OK : ESP_FAIL;
}
