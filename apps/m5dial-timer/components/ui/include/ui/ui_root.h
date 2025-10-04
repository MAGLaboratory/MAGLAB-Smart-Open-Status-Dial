#pragma once

#include <lvgl.h>

#include "esp_err.h"

#include "timer/timer_types.h"

namespace dial {

struct UiConfig {
    uint16_t screen_width = 360;
    uint16_t screen_height = 360;
};

class UiRoot {
public:
    esp_err_t init(const UiConfig& config);
    void update(const TimerSnapshot& snapshot);

private:
    void create_layout();
    void update_readout(const TimerSnapshot& snapshot);
    void update_progress(const TimerSnapshot& snapshot);

    UiConfig config_{};
    lv_obj_t* root_ = nullptr;
    lv_obj_t* label_time_ = nullptr;
    lv_obj_t* arc_progress_ = nullptr;
};

extern UiRoot g_ui_root;

}  // namespace dial
