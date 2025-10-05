#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_err.h"

namespace dial {

enum class TouchEventType : uint8_t {
    Tap,
    DoubleTap,
    LongPress,
    SwipeUp,
    SwipeDown,
    SwipeLeft,
    SwipeRight,
    TwoFingerTap,
};

enum class TapZone : uint8_t {
    Center,
    TopEdge,
    BottomEdge,
    LeftEdge,
    RightEdge,
};

struct TouchEvent {
    TouchEventType type = TouchEventType::Tap;
    TapZone zone = TapZone::Center;
    uint16_t x = 0;
    uint16_t y = 0;
    uint32_t duration_ms = 0;
};

struct TouchConfig {
    uint32_t poll_interval_ms = 10;
    uint32_t tap_max_duration_ms = 400;
    uint32_t double_tap_max_interval_ms = 350;
    uint32_t long_press_min_duration_ms = 600;
    uint16_t tap_max_movement = 40;  // pixels in controller space
    uint16_t swipe_min_distance = 60;
    uint32_t swipe_max_duration_ms = 700;
    uint8_t edge_zone_percent = 20;
    uint16_t screen_width = 240;
    uint16_t screen_height = 240;
    uint32_t queue_depth = 8;
};

class TouchInput {
public:
    TouchInput() = default;

    esp_err_t init(const TouchConfig& config = {});
    QueueHandle_t queue() const { return queue_; }

private:
    static void task_entry(void* arg);
    void run();

    TouchEvent make_tap_event(uint16_t x, uint16_t y, uint32_t duration_ms) const;
    TapZone classify_zone(uint16_t x, uint16_t y) const;

    TouchConfig config_{};
    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    bool initialized_ = false;

    bool last_active_ = false;
    bool multi_active_ = false;
    bool long_press_reported_ = false;
    uint16_t start_x_ = 0;
    uint16_t start_y_ = 0;
    uint16_t last_x_ = 0;
    uint16_t last_y_ = 0;
    uint8_t last_touch_count_ = 0;
    uint64_t start_us_ = 0;

    uint16_t multi_start_x_ = 0;
    uint16_t multi_start_y_ = 0;
    uint64_t multi_start_us_ = 0;

    bool pending_tap_ = false;
    TouchEvent pending_tap_event_{};
    uint64_t pending_tap_deadline_us_ = 0;
    uint64_t pending_tap_timestamp_us_ = 0;
};

extern TouchInput g_touch_input;

}  // namespace dial
