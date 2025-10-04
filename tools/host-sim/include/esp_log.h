#pragma once

#include <cstdio>
#include <cstdarg>

inline void esp_log_print(const char* level, const char* tag, const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    std::fprintf(stdout, "%s (%s): ", level, tag ? tag : "");
    std::vfprintf(stdout, fmt, args);
    std::fprintf(stdout, "\n");
    std::fflush(stdout);
    va_end(args);
}

#ifndef ESP_LOGI
#define ESP_LOGI(tag, fmt, ...) esp_log_print("I", tag, fmt, ##__VA_ARGS__)
#endif

#ifndef ESP_LOGW
#define ESP_LOGW(tag, fmt, ...) esp_log_print("W", tag, fmt, ##__VA_ARGS__)
#endif

#ifndef ESP_LOGE
#define ESP_LOGE(tag, fmt, ...) esp_log_print("E", tag, fmt, ##__VA_ARGS__)
#endif

#ifndef ESP_LOGD
#define ESP_LOGD(tag, fmt, ...) esp_log_print("D", tag, fmt, ##__VA_ARGS__)
#endif
