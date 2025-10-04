#pragma once

#include <esp_err.h>

namespace dial {

esp_err_t init_lvgl_display();
void lvgl_acquire();
void lvgl_release();

}  // namespace dial
