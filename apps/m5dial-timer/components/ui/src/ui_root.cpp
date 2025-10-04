#include "ui/ui_root.h"

#include <algorithm>

#include <esp_log.h>

namespace dial {

namespace {
constexpr const char* TAG = "UiRoot";

const lv_font_t* select_font(uint32_t /*seconds*/) {
    return LV_FONT_DEFAULT;
}

lv_color_t determine_color(const TimerSnapshot& snapshot) {
    if (snapshot.setpoint_seconds == 0) {
        return lv_color_hex(0x2ECC71);  // default green
    }

    const float total = static_cast<float>(snapshot.setpoint_seconds);
    float remaining = static_cast<float>(snapshot.remaining_seconds);

    float fraction = total > 0 ? remaining / total : 0.0f;

    if (remaining <= 60) {
        fraction = std::min(fraction, 0.01f);
    } else if (remaining <= 120) {
        fraction = std::min(fraction, 0.02f);
    } else if (remaining <= 300) {
        fraction = std::min(fraction, 0.05f);
    } else if (remaining <= 600) {
        fraction = std::min(fraction, 0.10f);
    }

    if (fraction <= 0.05f) {
        return lv_color_hex(0xE74C3C);  // red
    }
    if (fraction <= 0.10f) {
        return lv_color_hex(0xF1C40F);  // yellow
    }
    return lv_color_hex(0x2ECC71);  // green
}

}  // namespace

UiRoot g_ui_root;

esp_err_t UiRoot::init(const UiConfig& config) {
    config_ = config;

    root_ = lv_obj_create(lv_scr_act());
    if (root_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create root object");
        return ESP_FAIL;
    }
    lv_obj_set_size(root_, config_.screen_width, config_.screen_height);
    lv_obj_center(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    create_layout();
    return ESP_OK;
}

void UiRoot::create_layout() {
    arc_progress_ = lv_arc_create(root_);
    lv_obj_set_size(arc_progress_, config_.screen_width - 20, config_.screen_height - 20);
    lv_obj_center(arc_progress_);
    lv_arc_set_bg_angles(arc_progress_, 0, 360);
    lv_arc_set_rotation(arc_progress_, 270);
    lv_arc_set_mode(arc_progress_, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(arc_progress_, 0, 360);
    lv_obj_remove_style(arc_progress_, nullptr, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc_progress_, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_progress_, 16, LV_PART_INDICATOR);

    label_time_ = lv_label_create(root_);
    lv_label_set_text(label_time_, "00:00");
    lv_obj_set_style_text_align(label_time_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label_time_);
}

void UiRoot::update(const TimerSnapshot& snapshot) {
    update_readout(snapshot);
    update_progress(snapshot);
}

void UiRoot::update_readout(const TimerSnapshot& snapshot) {
    if (label_time_ == nullptr) {
        return;
    }

    char buffer[16];
    if (snapshot.setpoint_seconds >= 3600) {
        const uint32_t hours = snapshot.remaining_seconds / 3600;
        const uint32_t minutes = (snapshot.remaining_seconds % 3600) / 60;
        const uint32_t seconds = snapshot.remaining_seconds % 60;
        snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u",
                 static_cast<unsigned int>(hours),
                 static_cast<unsigned int>(minutes),
                 static_cast<unsigned int>(seconds));
    } else {
        const uint32_t minutes = snapshot.remaining_seconds / 60;
        const uint32_t seconds = snapshot.remaining_seconds % 60;
        snprintf(buffer, sizeof(buffer), "%02u:%02u",
                 static_cast<unsigned int>(minutes),
                 static_cast<unsigned int>(seconds));
    }
    lv_label_set_text(label_time_, buffer);
    lv_obj_set_style_text_color(label_time_, determine_color(snapshot), 0);
    lv_obj_set_style_text_font(label_time_, select_font(snapshot.remaining_seconds), 0);
}

void UiRoot::update_progress(const TimerSnapshot& snapshot) {
    if (arc_progress_ == nullptr) {
        return;
    }

    const uint32_t total = snapshot.setpoint_seconds == 0 ? 1 : snapshot.setpoint_seconds;
    const uint32_t remaining = snapshot.remaining_seconds;
    const int16_t sweep = static_cast<int16_t>((360 * remaining) / total);

    lv_arc_set_value(arc_progress_, sweep);
    lv_obj_set_style_arc_color(arc_progress_, determine_color(snapshot), LV_PART_INDICATOR);
}

}  // namespace dial
