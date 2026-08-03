#include "system_time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_sntp.h"

#define LOG_TAG "system_time"
#include "platform_log.h"

#define SYSTEM_TIME_TIMEZONE "CST-8"
#define SYSTEM_TIME_SNTP_PRIMARY "ntp.aliyun.com"
#define SYSTEM_TIME_SNTP_SECONDARY "time.cloudflare.com"

static void system_time_sntp_sync_callback(struct timeval *time_value)
{
    struct tm local_time;
    if (localtime_r(&time_value->tv_sec, &local_time) != NULL) {
        log_info("SNTP synchronized: %04d-%02d-%02d %02d:%02d:%02d",
                 local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday,
                 local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
    }
}

static int system_time_parse_month(const char *month)
{
    static const char *const month_names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };

    for (size_t index = 0; index < sizeof(month_names) / sizeof(month_names[0]); ++index) {
        if (strncmp(month, month_names[index], 3) == 0) {
            return (int)index;
        }
    }
    return -1;
}

esp_err_t system_time_init(void)
{
    if (setenv("TZ", SYSTEM_TIME_TIMEZONE, 1) != 0) {
        return ESP_FAIL;
    }
    tzset();

    char month_text[4] = {0};
    int day;
    int year;
    int hour;
    int minute;
    int second;

    if (sscanf(__DATE__, "%3s %d %d", month_text, &day, &year) != 3 ||
        sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) != 3) {
        return ESP_FAIL;
    }

    const int month = system_time_parse_month(month_text);
    if (month < 0) {
        return ESP_FAIL;
    }

    struct tm compiled_time = {
        .tm_year = year - 1900,
        .tm_mon = month,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = minute,
        .tm_sec = second,
        .tm_isdst = -1,
    };
    const time_t epoch = mktime(&compiled_time);
    if (epoch == (time_t)-1) {
        return ESP_FAIL;
    }

    const struct timeval time_value = {
        .tv_sec = epoch,
        .tv_usec = 0,
    };
    if (settimeofday(&time_value, NULL) != 0) {
        return ESP_FAIL;
    }

    log_info("system time initialized from build time: %s %s, timezone: %s",
             __DATE__, __TIME__, SYSTEM_TIME_TIMEZONE);
    return ESP_OK;
}

esp_err_t system_time_sntp_start(void)
{
    if (esp_sntp_enabled()) {
        if (!esp_sntp_restart()) {
            return ESP_ERR_INVALID_STATE;
        }
        log_info("SNTP synchronization restarted");
        return ESP_OK;
    }

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, SYSTEM_TIME_SNTP_PRIMARY);
    esp_sntp_setservername(1, SYSTEM_TIME_SNTP_SECONDARY);
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_set_time_sync_notification_cb(system_time_sntp_sync_callback);
    esp_sntp_init();
    log_info("SNTP synchronization started: %s, %s",
             SYSTEM_TIME_SNTP_PRIMARY, SYSTEM_TIME_SNTP_SECONDARY);
    return ESP_OK;
}
