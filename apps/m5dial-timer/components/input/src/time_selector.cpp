#include "input/time_selector.h"

#include <algorithm>
#include <cmath>

#include <freertos/task.h>
#include <esp_log.h>
#include <esp_timer.h>

namespace dial {

namespace {
constexpr const char* TAG = "TimeSelector";
}

TimeSelector g_time_selector;

esp_err_t TimeSelector::init(const TimeSelectorConfig& config) {
    config_ = config;

    if (event_queue_ == nullptr) {
        event_queue_ = xQueueCreate(config.queue_depth, sizeof(TimeDeltaEvent));
        if (event_queue_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create event queue");
            return ESP_ERR_NO_MEM;
        }
    }

    accumulated_seconds_ = std::min<int32_t>(config_.base_step_seconds, config_.max_total_seconds);
    last_timestamp_us_ = 0;
    last_activity_us_ = 0;
    commit_sent_ = false;
    return ESP_OK;
}

void TimeSelector::start() {
    if (task_handle_ != nullptr) {
        ESP_LOGW(TAG, "Time selector already running");
        return;
    }

    const BaseType_t res = xTaskCreatePinnedToCore(
        &TimeSelector::task_entry,
        "time_selector",
        4096,
        this,
        6,
        &task_handle_,
        0);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create time selector task");
        task_handle_ = nullptr;
    }
}

void TimeSelector::task_entry(void* arg) {
    auto* self = static_cast<TimeSelector*>(arg);
    self->run();
}

void TimeSelector::run() {
    EncoderSample sample;
    const TickType_t wait_ticks = pdMS_TO_TICKS(10);
    while (true) {
        if (xQueueReceive(g_encoder_reader.queue(), &sample, wait_ticks) == pdTRUE) {
            commit_sent_ = false;
            last_activity_us_ = static_cast<uint64_t>(sample.timestamp_us);
            process_sample(sample);
        } else {
            if (!commit_sent_ && last_activity_us_ != 0) {
                const uint64_t now_us = esp_timer_get_time();
                const uint64_t elapsed_ms = (now_us - last_activity_us_) / 1000ULL;
                if (elapsed_ms >= config_.commit_timeout_ms) {
                    TimeDeltaEvent commit_event{};
                    commit_event.type = TimeEventType::Commit;
                    commit_event.total_seconds = accumulated_seconds_;
                    commit_event.delta_seconds = 0;
                    commit_event.timestamp_us = static_cast<uint32_t>(now_us);
                    commit_event.multiplier = 0;
                    if (event_queue_ != nullptr) {
                        xQueueSend(event_queue_, &commit_event, portMAX_DELAY);
                    }
                    commit_sent_ = true;
                }
            }
        }
    }
}

void TimeSelector::process_sample(const EncoderSample& sample) {
    if (sample.delta_ticks == 0) {
        return;
    }

    const uint32_t now_us = sample.timestamp_us;
    const uint32_t dt_us = last_timestamp_us_ == 0 ? 0 : (now_us - last_timestamp_us_);
    last_timestamp_us_ = now_us;

    float ticks_per_second = 0.0f;
    if (dt_us > 0) {
        ticks_per_second = std::abs(static_cast<float>(sample.delta_ticks)) / (static_cast<float>(dt_us) / 1'000'000.0f);
    }

    uint32_t multiplier = 1;
    if (ticks_per_second >= config_.fast_threshold_tps) {
        multiplier = config_.fast_multiplier;
    } else if (ticks_per_second >= config_.medium_threshold_tps) {
        multiplier = config_.medium_multiplier;
    }

    const int32_t delta_seconds = static_cast<int32_t>(sample.delta_ticks) * static_cast<int32_t>(config_.base_step_seconds) * static_cast<int32_t>(multiplier);
    const int32_t previous = accumulated_seconds_;
    int32_t next = previous + delta_seconds;
    next = std::clamp(next, static_cast<int32_t>(0), static_cast<int32_t>(config_.max_total_seconds));

    if (next == previous) {
        return;
    }

    accumulated_seconds_ = next;
    const int32_t applied_delta = accumulated_seconds_ - previous;

    TimeDeltaEvent event{};
    event.type = TimeEventType::Delta;
    event.total_seconds = accumulated_seconds_;
    event.delta_seconds = applied_delta;
    event.timestamp_us = sample.timestamp_us;
    event.multiplier = multiplier;

    if (event_queue_ != nullptr) {
        xQueueSend(event_queue_, &event, portMAX_DELAY);
    }
}

}  // namespace dial
