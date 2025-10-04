#pragma once

#include <cstdint>

typedef int32_t esp_err_t;

#ifndef ESP_OK
#define ESP_OK 0
#endif

#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif

#ifndef ESP_ERR_NO_MEM
#define ESP_ERR_NO_MEM 0x101
#endif

#ifndef ESP_ERR_INVALID_STATE
#define ESP_ERR_INVALID_STATE 0x102
#endif

inline const char* esp_err_to_name(esp_err_t err) {
    switch (err) {
        case ESP_OK:
            return "ESP_OK";
        case ESP_FAIL:
            return "ESP_FAIL";
        case ESP_ERR_NO_MEM:
            return "ESP_ERR_NO_MEM";
        case ESP_ERR_INVALID_STATE:
            return "ESP_ERR_INVALID_STATE";
        default:
            return "ESP_ERR_UNKNOWN";
    }
}
