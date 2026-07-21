#include "system_time.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define LOG_TAG "system_time"
#include "platform_log.h"

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

    log_info("system time initialized from build time: %s %s", __DATE__, __TIME__);
    return ESP_OK;
}

esp_err_t system_time_sntp_start(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}
