#include "platform_log.h"

#include "../../greatlogger/glog.h"

#include <stdarg.h>
#include <stdio.h>

#define PLATFORM_LOG_INFO_BUFFER_SIZE 256

void log_verbose(const char *format, ...)
{
    char    message[PLATFORM_LOG_INFO_BUFFER_SIZE];
    va_list args;

    va_start(args, format);
    int length = vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (length < 0) {
        return;
    }

    size_t message_length = (size_t)length;
    if (message_length >= sizeof(message)) {
        message_length = sizeof(message) - 1;
    }

    //ESP_LOGI(LOG_TAG, "%s", message);
    (void)glog_put(message, message_length);
}
