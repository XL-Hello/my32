#include "ui_font.h"

#define LOG_TAG "ui_font"
#include "platform_log.h"

#define UI_FONT_8_PATH  "R:/littlefs/fonts/esp_front_8.bin"
#define UI_FONT_9_PATH  "R:/littlefs/fonts/esp_front_9.bin"
#define UI_FONT_11_PATH "R:/littlefs/fonts/esp_front_11.bin"
#define UI_FONT_12_PATH "R:/littlefs/fonts/esp_front_12.bin"
#define UI_FONT_13_PATH "R:/littlefs/fonts/esp_front_13.bin"
#define UI_FONT_14_PATH "R:/littlefs/fonts/esp_front_14.bin"
#define UI_FONT_15_PATH "R:/littlefs/fonts/esp_front_15.bin"
#define UI_FONT_16_PATH "R:/littlefs/fonts/esp_front_16.bin"
#define UI_FONT_20_PATH "R:/littlefs/fonts/esp_front_20.bin"

static lv_font_t *s_font_8;
static lv_font_t *s_font_9;
static lv_font_t *s_font_11;
static lv_font_t *s_font_12;
static lv_font_t *s_font_13;
static lv_font_t *s_font_14;
static lv_font_t *s_font_15;
static lv_font_t *s_font_16;
static lv_font_t *s_font_20;

bool ui_font_init(void)
{
    if (s_font_8 != NULL && s_font_9 != NULL && s_font_11 != NULL && s_font_12 != NULL &&
        s_font_13 != NULL && s_font_14 != NULL && s_font_15 != NULL && s_font_16 != NULL &&
        s_font_20 != NULL) {
        return true;
    }

    s_font_8 = lv_font_load(UI_FONT_8_PATH);
    s_font_9 = lv_font_load(UI_FONT_9_PATH);
    s_font_11 = lv_font_load(UI_FONT_11_PATH);
    s_font_12 = lv_font_load(UI_FONT_12_PATH);
    s_font_13 = lv_font_load(UI_FONT_13_PATH);
    s_font_14 = lv_font_load(UI_FONT_14_PATH);
    s_font_15 = lv_font_load(UI_FONT_15_PATH);
    s_font_16 = lv_font_load(UI_FONT_16_PATH);
    s_font_20 = lv_font_load(UI_FONT_20_PATH);
    if (s_font_8 == NULL || s_font_9 == NULL || s_font_11 == NULL || s_font_12 == NULL ||
        s_font_13 == NULL || s_font_14 == NULL || s_font_15 == NULL || s_font_16 == NULL ||
        s_font_20 == NULL) {
        log_error("Failed to load UI fonts from LittleFS");
        lv_font_free(s_font_8);
        lv_font_free(s_font_9);
        lv_font_free(s_font_11);
        lv_font_free(s_font_12);
        lv_font_free(s_font_13);
        lv_font_free(s_font_14);
        lv_font_free(s_font_15);
        lv_font_free(s_font_16);
        lv_font_free(s_font_20);
        s_font_8 = NULL;
        s_font_9 = NULL;
        s_font_11 = NULL;
        s_font_12 = NULL;
        s_font_13 = NULL;
        s_font_14 = NULL;
        s_font_15 = NULL;
        s_font_16 = NULL;
        s_font_20 = NULL;
        return false;
    }

    log_info("Loaded UI fonts from LittleFS");
    return true;
}

const lv_font_t *ui_font_get_8(void)
{
    return s_font_8;
}

const lv_font_t *ui_font_get_9(void)
{
    return s_font_9;
}

const lv_font_t *ui_font_get_11(void)
{
    return s_font_11;
}

const lv_font_t *ui_font_get_12(void)
{
    return s_font_12;
}

const lv_font_t *ui_font_get_13(void)
{
    return s_font_13;
}

const lv_font_t *ui_font_get_14(void)
{
    return s_font_14;
}

const lv_font_t *ui_font_get_15(void)
{
    return s_font_15;
}

const lv_font_t *ui_font_get_16(void)
{
    return s_font_16;
}

const lv_font_t *ui_font_get_20(void)
{
    return s_font_20;
}
