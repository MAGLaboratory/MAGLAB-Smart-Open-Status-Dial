#include "input/touch_input.h"

#include <algorithm>
#include <cstdlib>

#include <esp_log.h>
#include <esp_timer.h>

#include "board/dial_board.h"

namespace dial {

namespace {
constexpr const char* TAG = "TouchInput";
}

TouchInput g_touch_input;

esp_err_t TouchInput::init(const TouchConfig& config) {
    if (initialized_) {
        return ESP_OK;
    }

    if (!g_board.config().enable_touch || !g_board.touch().initialized()) {
        ESP_LOGW(TAG, "Touch controller not available; skipping touch input init");
        return ESP_ERR_INVALID_STATE;
    }

    config_ = config;

    if (queue_ == nullptr) {
        queue_ = xQueueCreate(config_.queue_depth, sizeof(TouchEvent));
        if (queue_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create touch event queue");
            return ESP_ERR_NO_MEM;
        }
    }

    if (task_handle_ == nullptr) {
        BaseType_t res = xTaskCreatePinnedToCore(
            &TouchInput::task_entry,
            "touch_input",
            3072,
            this,
            5,
            &task_handle_,
            1);
        if (res != pdPASS) {
            task_handle_ = nullptr;
            ESP_LOGE(TAG, "Failed to create touch input task");
            return ESP_FAIL;
        }
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Touch input initialised");
    return ESP_OK;
}

void TouchInput::task_entry(void* arg) {
    auto* self = static_cast<TouchInput*>(arg);
    self->run();
}

void TouchInput::run() {
    const TickType_t delay_ticks = pdMS_TO_TICKS(std::max<uint32_t>(1, config_.poll_interval_ms));

    TouchPoint point{};
    while (true) {
        bool touch_ok = g_board.touch().read(&point) == ESP_OK;
        const bool active = touch_ok && point.touched;

        if (active) {
            last_x_ = point.x;
            last_y_ = point.y;
            if (!last_active_) {
                start_x_ = point.x;
                start_y_ = point.y;
                start_us_ = esp_timer_get_time();
                last_active_ = true;
            }
        } else if (last_active_) {
            const uint64_t end_us = esp_timer_get_time();
            const uint32_t duration_ms = static_cast<uint32_t>((end_us - start_us_) / 1000ULL);
            const uint16_t dx = static_cast<uint16_t>(std::abs(static_cast<int32_t>(last_x_) - static_cast<int32_t>(start_x_)));
            const uint16_t dy = static_cast<uint16_t>(std::abs(static_cast<int32_t>(last_y_) - static_cast<int32_t>(start_y_)));
            const uint16_t movement = std::max(dx, dy);

            if (duration_ms <= config_.tap_max_duration_ms && movement <= config_.tap_max_movement) {
                TouchEvent event{};
                event.type = TouchEventType::Tap;
                event.x = start_x_;
                event.y = start_y_;
                event.duration_ms = duration_ms;
                if (queue_ != nullptr) {
                    xQueueSend(queue_, &event, 0);
                }
            }

            last_active_ = false;
        }

        vTaskDelay(delay_ticks);
    }
}

}  // namespace dial

