#include "sys_info_ui.h"

#include <stdio.h>

#include "controlcenter_ui.h"
#include "icon.h"
#include "local_ota.h"
#include "lvgl/lvgl.h"
#include "ui_font.h"

#define COLOR_BACKGROUND 0x10201F
#define COLOR_CARD 0x19302E
#define COLOR_CARD_END 0x1B3733
#define COLOR_CARD_BORDER 0x29524C
#define COLOR_ICON_BASE 0x132926
#define COLOR_ACCENT 0x58D6B3
#define COLOR_PRIMARY 0xF2FAF7
#define COLOR_SECONDARY 0x9BB9B0
#define COLOR_SUCCESS 0x7CE3C6
#define COLOR_WARNING 0xF2B766

#define SYS_INFO_STATUS_PERIOD_MS 120U

static const sys_info_device_info_t s_default_info = {
    .model = "Dreame ESP32-S3",
    .os_version = "v1.0.0 · 当前稳定版",
    .storage = "8 MB Flash · 2 MB LittleFS",
};
static sys_info_device_info_t s_device_info;
static lv_obj_t *s_update_card;
static lv_obj_t *s_update_icon;
static lv_obj_t *s_update_title;
static lv_obj_t *s_update_detail;
static lv_obj_t *s_update_chevron;
static lv_obj_t *s_update_progress;
static lv_obj_t *s_update_percent;
static lv_timer_t *s_status_timer;
static sys_info_update_state_t s_displayed_state;

static void set_icon_color(lv_obj_t *image, uint32_t color)
{
    lv_obj_set_style_img_recolor(image, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(image, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *create_icon(lv_obj_t *parent, const char *path)
{
    lv_obj_t *image = lv_img_create(parent);
    ui_icon_set_src(image, path);
    set_icon_color(image, COLOR_ACCENT);
    return image;
}

static void create_icon_base(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const char *path)
{
    lv_obj_t *base = lv_obj_create(parent);
    lv_obj_set_size(base, 26, 26);
    lv_obj_set_pos(base, x, y);
    lv_obj_set_style_bg_color(base, lv_color_hex(COLOR_ICON_BASE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(base, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(base, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(base, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(base, 0, LV_PART_MAIN);
    lv_obj_clear_flag(base, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *icon = create_icon(base, path);
    lv_obj_center(icon);
}

static void style_card(lv_obj_t *card, lv_coord_t height)
{
    lv_obj_set_size(card, 216, height);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(card, lv_color_hex(COLOR_CARD_END), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_HOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_CARD_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 7, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
}

static void create_info_card(lv_obj_t *parent, lv_coord_t y, const char *icon_path,
                             const char *title, const char *detail)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, 12, y);
    style_card(card, 43);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
    create_icon_base(card, 12, 8, icon_path);
    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, 46, 4);
    lv_obj_set_style_text_font(title_label, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_t *detail_label = lv_label_create(card);
    lv_label_set_text(detail_label, detail);
    lv_label_set_long_mode(detail_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(detail_label, 148, 13);
    lv_obj_set_pos(detail_label, 46, 27);
    lv_obj_set_style_text_font(detail_label, ui_font_get_9(), LV_PART_MAIN);
    lv_obj_set_style_text_color(detail_label, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
}

static void update_card_clicked(lv_event_t *event)
{
    (void)event;
    switch (s_displayed_state) {
    case SYS_INFO_UPDATE_NO_UPDATE:
    case SYS_INFO_UPDATE_FAILED:
        local_ota_check_async();
        break;
    case SYS_INFO_UPDATE_AVAILABLE:
        local_ota_download_async();
        break;
    case SYS_INFO_UPDATE_READY_TO_INSTALL:
        local_ota_install_async();
        break;
    default:
        break;
    }
}

void sys_info_ui_update_progress(size_t received_bytes, size_t total_bytes)
{
    if (s_update_progress == NULL || total_bytes == 0) return;
    uint32_t percent = (uint32_t)((received_bytes * 100U) / total_bytes);
    if (percent > 100U) percent = 100U;
    lv_bar_set_value(s_update_progress, (int32_t)percent, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_update_percent, "%u%%", (unsigned)percent);
}

void sys_info_ui_set_update_state(sys_info_update_state_t state, const char *file_name,
                                  const char *detail)
{
    if (s_update_card == NULL || !lv_obj_is_valid(s_update_card)) return;
    const char *title = "更新检测";
    const char *subtext = "无更新 · 轻触检查";
    const char *icon = UI_ICON_PATH_UPDATE;
    uint32_t title_color = COLOR_PRIMARY;
    bool downloading = false;
    bool highlighted = false;
    switch (state) {
    case SYS_INFO_UPDATE_CHECKING: title = "正在检查"; subtext = "正在获取更新信息"; break;
    case SYS_INFO_UPDATE_AVAILABLE: title = "发现新版本"; subtext = file_name; icon = UI_ICON_PATH_DOWNLOAD; title_color = COLOR_SUCCESS; highlighted = true; break;
    case SYS_INFO_UPDATE_DOWNLOADING: title = "正在下载"; subtext = file_name; icon = UI_ICON_PATH_DOWNLOAD; downloading = true; break;
    case SYS_INFO_UPDATE_READY_TO_INSTALL: title = "可安装"; subtext = file_name; icon = UI_ICON_PATH_INSTALL; title_color = COLOR_SUCCESS; highlighted = true; break;
    case SYS_INFO_UPDATE_INSTALLING: title = "正在升级"; subtext = "请勿断电"; icon = UI_ICON_PATH_INSTALL; break;
    case SYS_INFO_UPDATE_FAILED: title = "更新失败"; subtext = detail; title_color = COLOR_WARNING; break;
    case SYS_INFO_UPDATE_NO_UPDATE: default: break;
    }
    s_displayed_state = state;
    ui_icon_set_src(s_update_icon, icon);
    set_icon_color(s_update_icon, COLOR_ACCENT);
    lv_label_set_text(s_update_title, title);
    lv_label_set_text(s_update_detail, subtext != NULL && subtext[0] != '\0' ? subtext : "轻触重试");
    lv_obj_set_style_text_color(s_update_title, lv_color_hex(title_color), LV_PART_MAIN);
    lv_obj_set_style_border_color(s_update_card, lv_color_hex(highlighted ? COLOR_ACCENT : COLOR_CARD_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_update_card, highlighted ? 1 : 1, LV_PART_MAIN);
    if (downloading) {
        lv_obj_add_flag(s_update_chevron, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_update_progress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_update_percent, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_update_chevron, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_update_progress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_update_percent, LV_OBJ_FLAG_HIDDEN);
    }
}

static void status_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    local_ota_status_t status;
    local_ota_get_status(&status);
    sys_info_update_state_t state = (sys_info_update_state_t)status.state;
    sys_info_ui_set_update_state(state, status.file_name, status.detail);
    if (state == SYS_INFO_UPDATE_DOWNLOADING) sys_info_ui_update_progress(status.received_bytes, status.total_bytes);
}

static void back_clicked(lv_event_t *event)
{
    (void)event;
    if (s_status_timer != NULL) { lv_timer_del(s_status_timer); s_status_timer = NULL; }
    s_update_card = NULL;
    controlcenter_ui_create();
}

static void create_header(lv_obj_t *screen)
{
    lv_obj_t *back = lv_btn_create(screen);
    lv_obj_set_size(back, 24, 24);
    lv_obj_set_pos(back, 16, 16);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(back, lv_color_hex(COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_width(back, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(back, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(back, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(back, back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_icon = create_icon(back, UI_ICON_PATH_BACK);
    lv_obj_center(back_icon);
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "系统信息");
    lv_obj_set_pos(title, 54, 17);
    lv_obj_set_style_text_font(title, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
}

static void create_update_card(lv_obj_t *screen)
{
    lv_obj_t *heading = lv_label_create(screen);
    lv_label_set_text(heading, "软件更新");
    lv_obj_set_pos(heading, 16, 216);
    lv_obj_set_style_text_font(heading, ui_font_get_9(), LV_PART_MAIN);
    lv_obj_set_style_text_color(heading, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
    s_update_card = lv_obj_create(screen);
    lv_obj_set_pos(s_update_card, 12, 233);
    style_card(s_update_card, 66);
    lv_obj_add_event_cb(s_update_card, update_card_clicked, LV_EVENT_CLICKED, NULL);
    create_icon_base(s_update_card, 12, 20, UI_ICON_PATH_UPDATE);
    s_update_icon = lv_obj_get_child(s_update_card, 0);
    /* 图标位于图标底座的第一个子对象。 */
    s_update_icon = lv_obj_get_child(s_update_icon, 0);
    s_update_title = lv_label_create(s_update_card);
    lv_obj_set_pos(s_update_title, 46, 10);
    lv_obj_set_style_text_font(s_update_title, ui_font_get_16(), LV_PART_MAIN);
    s_update_detail = lv_label_create(s_update_card);
    lv_label_set_long_mode(s_update_detail, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(s_update_detail, 140, 13);
    lv_obj_set_pos(s_update_detail, 46, 39);
    lv_obj_set_style_text_font(s_update_detail, ui_font_get_9(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_update_detail, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
    s_update_chevron = create_icon(s_update_card, UI_ICON_PATH_CHEVRON_RIGHT);
    lv_obj_set_pos(s_update_chevron, 190, 21);
    s_update_progress = lv_bar_create(s_update_card);
    lv_obj_set_size(s_update_progress, 146, 5);
    lv_obj_set_pos(s_update_progress, 46, 48);
    lv_bar_set_range(s_update_progress, 0, 100);
    lv_obj_set_style_bg_color(s_update_progress, lv_color_hex(0x274844), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_update_progress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_update_progress, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_update_progress, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(s_update_progress, 3, LV_PART_INDICATOR);
    s_update_percent = lv_label_create(s_update_card);
    lv_obj_set_pos(s_update_percent, 170, 39);
    lv_obj_set_style_text_font(s_update_percent, ui_font_get_9(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_update_percent, lv_color_hex(COLOR_SUCCESS), LV_PART_MAIN);
    sys_info_ui_set_update_state(SYS_INFO_UPDATE_NO_UPDATE, NULL, NULL);
}

void sys_info_ui_set_device_info(const sys_info_device_info_t *device_info)
{
    s_device_info = device_info == NULL ? s_default_info : *device_info;
}

void sys_info_ui_create(void)
{
    if (s_status_timer != NULL) { lv_timer_del(s_status_timer); s_status_timer = NULL; }
    if (s_device_info.model == NULL) s_device_info = s_default_info;
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    create_header(screen);
    create_info_card(screen, 61, UI_ICON_PATH_CHIP, "本机型号", s_device_info.model);
    create_info_card(screen, 112, UI_ICON_PATH_OS, "OS 版本", s_device_info.os_version);
    create_info_card(screen, 163, UI_ICON_PATH_STORAGE, "机身内存", s_device_info.storage);
    create_update_card(screen);
    s_status_timer = lv_timer_create(status_timer_cb, SYS_INFO_STATUS_PERIOD_MS, NULL);
    status_timer_cb(s_status_timer);
}
