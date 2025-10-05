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
            4096,
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

TapZone TouchInput::classify_zone(uint16_t x, uint16_t y) const {
    const uint16_t top_threshold = static_cast<uint16_t>((config_.screen_height * config_.edge_zone_percent) / 100);
    const uint16_t bottom_threshold = static_cast<uint16_t>(config_.screen_height - top_threshold);
    const uint16_t left_threshold = static_cast<uint16_t>((config_.screen_width * config_.edge_zone_percent) / 100);
    const uint16_t right_threshold = static_cast<uint16_t>(config_.screen_width - left_threshold);

    if (y <= top_threshold) {
        return TapZone::TopEdge;
    }
    if (y >= bottom_threshold) {
        return TapZone::BottomEdge;
    }
    if (x <= left_threshold) {
        return TapZone::LeftEdge;
    }
    if (x >= right_threshold) {
        return TapZone::RightEdge;
    }
    return TapZone::Center;
}

TouchEvent TouchInput::make_tap_event(uint16_t x, uint16_t y, uint32_t duration_ms) const {
    TouchEvent event{};
    event.type = TouchEventType::Tap;
    event.zone = classify_zone(x, y);
    event.x = x;
    event.y = y;
    event.duration_ms = duration_ms;
    return event;
}

void TouchInput::run() {
    const TickType_t delay_ticks = pdMS_TO_TICKS(std::max<uint32_t>(1, config_.poll_interval_ms));

    TouchPoint point{};
    while (true) {
        const uint64_t now_us = esp_timer_get_time();

        if (pending_tap_ && !last_active_ && !multi_active_ && now_us >= pending_tap_deadline_us_) {
            if (queue_ != nullptr) {
                xQueueSend(queue_, &pending_tap_event_, 0);
            }
            pending_tap_ = false;
            pending_tap_timestamp_us_ = 0;
            pending_tap_deadline_us_ = 0;
        }

        bool touch_ok = g_board.touch().read(&point) == ESP_OK;
        const bool active = touch_ok && point.touched;
        const uint8_t touches = touch_ok ? point.touch_count : 0;

        if (touches >= 2) {
            if (!multi_active_) {
                multi_active_ = true;
                multi_start_x_ = point.x;
                multi_start_y_ = point.y;
                multi_start_us_ = now_us;
            }
            last_active_ = false;
        }

        if (active && touches == 1 && !multi_active_) {
            last_x_ = point.x;
            last_y_ = point.y;
            if (!last_active_) {
                start_x_ = point.x;
                start_y_ = point.y;
                start_us_ = now_us;
                long_press_reported_ = false;
            } else if (!long_press_reported_) {
                const uint32_t duration_ms = static_cast<uint32_t>((now_us - start_us_) / 1000ULL);
                if (duration_ms >= config_.long_press_min_duration_ms) {
                    TouchEvent event{};
                    event.type = TouchEventType::LongPress;
                    event.zone = classify_zone(start_x_, start_y_);
                    event.x = start_x_;
                    event.y = start_y_;
                    event.duration_ms = duration_ms;
                    if (queue_ != nullptr) {
                        xQueueSend(queue_, &event, 0);
                    }
                    long_press_reported_ = true;
                }
            }
            last_active_ = true;
        } else if (!active) {
            if (multi_active_) {
                TouchEvent event{};
                event.type = TouchEventType::TwoFingerTap;
                event.x = multi_start_x_;
                event.y = multi_start_y_;
                event.duration_ms = static_cast<uint32_t>((now_us - multi_start_us_) / 1000ULL);
                if (queue_ != nullptr) {
                    xQueueSend(queue_, &event, 0);
                }
                multi_active_ = false;
                last_active_ = false;
                pending_tap_ = false;
                pending_tap_timestamp_us_ = 0;
                pending_tap_deadline_us_ = 0;
                continue;
            }

            if (last_active_) {
                const uint32_t duration_ms = static_cast<uint32_t>((now_us - start_us_) / 1000ULL);
                const int32_t dx = static_cast<int32_t>(last_x_) - static_cast<int32_t>(start_x_);
                const int32_t dy = static_cast<int32_t>(last_y_) - static_cast<int32_t>(start_y_);
                const uint16_t movement = static_cast<uint16_t>(std::max(std::abs(dx), std::abs(dy)));

                const bool is_swipe = movement >= config_.swipe_min_distance && duration_ms <= config_.swipe_max_duration_ms;
                const bool is_tap = movement <= config_.tap_max_movement && duration_ms <= config_.tap_max_duration_ms;

                if (is_swipe) {
                    TouchEvent event{};
                    if (std::abs(dy) >= std::abs(dx)) {
                        event.type = dy < 0 ? TouchEventType::SwipeUp : TouchEventType::SwipeDown;
                    } else {
                        event.type = dx < 0 ? TouchEventType::SwipeLeft : TouchEventType::SwipeRight;
                    }
                    event.x = start_x_;
                    event.y = start_y_;
                    event.duration_ms = duration_ms;
                    if (queue_ != nullptr) {
                        xQueueSend(queue_, &event, 0);
                    }
                    pending_tap_ = false;
                } else if (!long_press_reported_ && is_tap) {
                    TouchEvent tap_event = make_tap_event(start_x_, start_y_, duration_ms);
                    if (pending_tap_ && (now_us - pending_tap_timestamp_us_) <= static_cast<uint64_t>(config_.double_tap_max_interval_ms) * 1000ULL) {
                        TouchEvent event{};
                        event.type = TouchEventType::DoubleTap;
                        event.zone = tap_event.zone;
                        event.x = tap_event.x;
                        event.y = tap_event.y;
                        event.duration_ms = tap_event.duration_ms;
                        if (queue_ != nullptr) {
                            xQueueSend(queue_, &event, 0);
                        }
                        pending_tap_ = false;
                        pending_tap_timestamp_us_ = 0;
                        pending_tap_deadline_us_ = 0;
                    } else {
                        pending_tap_event_ = tap_event;
                        pending_tap_timestamp_us_ = now_us;
                        pending_tap_deadline_us_ = now_us + static_cast<uint64_t>(config_.double_tap_max_interval_ms) * 1000ULL;
                        pending_tap_ = true;
                    }
                }
            }

            last_active_ = false;
        }

        last_touch_count_ = touches;
        vTaskDelay(delay_ticks);
    }
}

}  // namespace dial
