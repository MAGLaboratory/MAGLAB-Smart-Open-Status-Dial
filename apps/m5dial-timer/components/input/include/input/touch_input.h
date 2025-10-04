#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_err.h"

namespace dial {

enum class TouchEventType : uint8_t {
    Tap,
};

struct TouchEvent {
    TouchEventType type = TouchEventType::Tap;
    uint16_t x = 0;
    uint16_t y = 0;
    uint32_t duration_ms = 0;
};

struct TouchConfig {
    uint32_t poll_interval_ms = 10;
    uint32_t tap_max_duration_ms = 400;
    uint16_t tap_max_movement = 40;  // pixels in controller space
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

    TouchConfig config_{};
    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    bool initialized_ = false;

    bool last_active_ = false;
    uint16_t start_x_ = 0;
    uint16_t start_y_ = 0;
    uint16_t last_x_ = 0;
    uint16_t last_y_ = 0;
    uint64_t start_us_ = 0;
};

extern TouchInput g_touch_input;

}  // namespace dial
