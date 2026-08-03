#include "icon.h"

#define LOG_TAG "ui_icon"
#include "platform_log.h"

bool ui_icon_set_src(lv_obj_t *image, const char *resource_path)
{
    if (image == NULL) {
        log_error("设置图标失败：图像对象为空");
        return false;
    }
    if (resource_path == NULL || resource_path[0] == '\0') {
        log_error("设置图标失败：资源路径为空");
        lv_img_set_src(image, NULL);
        return false;
    }

    lv_fs_file_t file;
    const lv_fs_res_t result = lv_fs_open(&file, resource_path, LV_FS_MODE_RD);
    if (result != LV_FS_RES_OK) {
        log_error("图标资源不可读取：%s，LVGL 文件系统错误=%d", resource_path, result);
        lv_img_set_src(image, NULL);
        return false;
    }
    lv_fs_close(&file);

    lv_img_set_src(image, resource_path);
    return true;
}
