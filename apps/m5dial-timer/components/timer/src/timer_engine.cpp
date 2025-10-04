#include "timer/timer_engine.h"

#include <algorithm>

#include <freertos/task.h>
#include <esp_log.h>
#include <esp_check.h>

#include "timer/state_machine.h"
#include "services/state_persistence.h"

namespace dial {

namespace {
constexpr const char* TAG = "TimerEngine";
constexpr uint32_t kTimerPeriodUs = 1000;  // 1 ms
}  // namespace

TimerEngine g_timer_engine;

esp_err_t TimerEngine::init(const TimerEngineConfig& config) {
    config_ = config;

    if (snapshot_queue_ == nullptr) {
        snapshot_queue_ = xQueueCreate(1, sizeof(TimerSnapshot));
        if (snapshot_queue_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create snapshot queue");
            return ESP_ERR_NO_MEM;
        }
    }

    if (delta_queue_ == nullptr) {
        delta_queue_ = xQueueCreate(16, sizeof(TimeDeltaEvent));
        if (delta_queue_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create delta queue");
            return ESP_ERR_NO_MEM;
        }
    }

    if (esp_timer_ == nullptr) {
        esp_timer_create_args_t args{
            .callback = &TimerEngine::timer_callback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "timer_engine",
            .skip_unhandled_events = true,
        };
        ESP_RETURN_ON_ERROR(esp_timer_create(&args, &esp_timer_), TAG, "esp_timer_create failed");
    }

    state_ = TimerState::Idle;
    setpoint_seconds_ = std::min(config_.max_total_seconds, static_cast<uint32_t>(15 * 60));
    remaining_ms_ = static_cast<int64_t>(setpoint_seconds_) * 1000;

    persistence::RestoredState restored;
    if (persistence::load(&restored) == ESP_OK && restored.valid) {
        setpoint_seconds_ = std::min(restored.setpoint_seconds, config_.max_total_seconds);
        remaining_ms_ = std::min<int64_t>(static_cast<int64_t>(restored.remaining_ms), static_cast<int64_t>(setpoint_seconds_) * 1000);
        if (setpoint_seconds_ == 0) {
            state_ = TimerState::Idle;
        } else if (restored.state == TimerState::Counting || restored.state == TimerState::Arming) {
            state_ = TimerState::Editing;
        } else {
            state_ = restored.state;
        }
    }

    publish_snapshot();

    return ESP_OK;
}

void TimerEngine::start() {
    if (task_handle_ != nullptr) {
        ESP_LOGW(TAG, "Timer engine already running");
        return;
    }

    const BaseType_t res = xTaskCreatePinnedToCore(
        &TimerEngine::task_entry,
        "timer_engine",
        4096,
        this,
        7,
        &task_handle_,
        0);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create timer engine task");
        task_handle_ = nullptr;
    }
}

void TimerEngine::task_entry(void* arg) {
    auto* self = static_cast<TimerEngine*>(arg);
    self->run();
}

void TimerEngine::enqueue_control(ControlCommand command) {
    if (delta_queue_ == nullptr) {
        return;
    }

    TimeDeltaEvent event{};
    event.type = TimeEventType::Control;
    event.total_seconds = static_cast<int32_t>(setpoint_seconds_);
    event.delta_seconds = 0;
    event.timestamp_us = static_cast<uint32_t>(esp_timer_get_time());
    event.multiplier = 0;
    event.control = command;
    xQueueSend(delta_queue_, &event, portMAX_DELAY);
}

void TimerEngine::run() {
    TickType_t last_publish = xTaskGetTickCount();
    const TickType_t publish_interval = pdMS_TO_TICKS(1000 / config_.snapshot_hz);

    TimeDeltaEvent event;
    while (true) {
        if (xQueueReceive(delta_queue_, &event, pdMS_TO_TICKS(5)) == pdTRUE) {
            if (event.type == TimeEventType::Control) {
                bool changed = false;
                switch (event.control) {
                    case ControlCommand::ToggleRun:
                        if (state_ == TimerState::Counting) {
                            if (esp_timer_is_active(esp_timer_)) {
                                esp_timer_stop(esp_timer_);
                            }
                            state_ = TimerState::Editing;
                            remaining_ms_ = static_cast<int64_t>(setpoint_seconds_) * 1000;
                            changed = true;
                        } else {
                            if (setpoint_seconds_ > 0) {
                                remaining_ms_ = static_cast<int64_t>(setpoint_seconds_) * 1000;
                                if (esp_timer_is_active(esp_timer_)) {
                                    esp_timer_stop(esp_timer_);
                                }
                                esp_timer_start_periodic(esp_timer_, kTimerPeriodUs);
                                state_ = TimerState::Counting;
                                changed = true;
                            }
                        }
                        break;
                    case ControlCommand::Reset:
                        if (setpoint_seconds_ != 0 || remaining_ms_ != 0 || state_ != TimerState::Idle) {
                            setpoint_seconds_ = 0;
                            remaining_ms_ = 0;
                            if (esp_timer_is_active(esp_timer_)) {
                                esp_timer_stop(esp_timer_);
                            }
                            state_ = TimerState::Idle;
                            changed = true;
                        }
                        break;
                    case ControlCommand::None:
                    default:
                        break;
                }

                if (changed) {
                    TimerSnapshot snapshot{
                        .state = state_,
                        .setpoint_seconds = setpoint_seconds_,
                        .remaining_seconds = static_cast<uint32_t>(remaining_ms_ <= 0 ? 0 : (remaining_ms_ / 1000)),
                        .remaining_ms = static_cast<uint32_t>(remaining_ms_ <= 0 ? 0 : remaining_ms_),
                        .monotonic_us = static_cast<uint64_t>(esp_timer_get_time()),
                    };
                    persistence::save(snapshot);
                    publish_snapshot();
                }
                continue;
            }

            if (event.type == TimeEventType::Delta) {
                int64_t updated = static_cast<int64_t>(setpoint_seconds_) + static_cast<int64_t>(event.delta_seconds);
                updated = std::max<int64_t>(
                    0,
                    std::min<int64_t>(updated, static_cast<int64_t>(config_.max_total_seconds)));
                setpoint_seconds_ = static_cast<uint32_t>(updated);
                remaining_ms_ = static_cast<int64_t>(setpoint_seconds_) * 1000;
            }

            state_ = determine_next_state(state_, event, config_.auto_start);

            if (state_ == TimerState::Finished || state_ == TimerState::Idle || setpoint_seconds_ == 0) {
                remaining_ms_ = static_cast<int64_t>(setpoint_seconds_) * 1000;
                if (esp_timer_is_active(esp_timer_)) {
                    esp_timer_stop(esp_timer_);
                }
            } else if (state_ == TimerState::Counting) {
                remaining_ms_ = static_cast<int64_t>(setpoint_seconds_) * 1000;
                if (esp_timer_is_active(esp_timer_)) {
                    esp_timer_stop(esp_timer_);
                }
                esp_timer_start_periodic(esp_timer_, kTimerPeriodUs);
            } else {
                if (esp_timer_is_active(esp_timer_)) {
                    esp_timer_stop(esp_timer_);
                }
            }

            if (event.type == TimeEventType::Commit || state_ == TimerState::Finished) {
                TimerSnapshot snapshot{
                    .state = state_,
                    .setpoint_seconds = setpoint_seconds_,
                    .remaining_seconds = static_cast<uint32_t>((remaining_ms_ < 0 ? 0 : remaining_ms_) / 1000),
                    .remaining_ms = static_cast<uint32_t>(remaining_ms_ < 0 ? 0 : remaining_ms_),
                    .monotonic_us = static_cast<uint64_t>(esp_timer_get_time()),
                };
                persistence::save(snapshot);
                publish_snapshot();
                continue;
            }
        }

        const TickType_t now = xTaskGetTickCount();
        if (now - last_publish >= publish_interval) {
            publish_snapshot();
            last_publish = now;
        }
    }
}

void TimerEngine::timer_callback(void* arg) {
    auto* self = static_cast<TimerEngine*>(arg);
    self->on_tick();
}

void TimerEngine::on_tick() {
    if (state_ != TimerState::Counting) {
        return;
    }

    remaining_ms_ -= 1;
    if (remaining_ms_ <= 0) {
        remaining_ms_ = 0;
        state_ = TimerState::Finished;
        setpoint_seconds_ = 0;
        if (esp_timer_is_active(esp_timer_)) {
            esp_timer_stop(esp_timer_);
        }
        TimerSnapshot snapshot{
            .state = state_,
            .setpoint_seconds = setpoint_seconds_,
            .remaining_seconds = 0,
            .remaining_ms = 0,
            .monotonic_us = static_cast<uint64_t>(esp_timer_get_time()),
        };
        persistence::save(snapshot);
        publish_snapshot();
    }
}

void TimerEngine::publish_snapshot() {
    if (snapshot_queue_ == nullptr) {
        return;
    }

    const int64_t remaining_ms_clamped = remaining_ms_ < 0 ? 0 : remaining_ms_;
    const TimerSnapshot snapshot{
        .state = state_,
        .setpoint_seconds = setpoint_seconds_,
        .remaining_seconds = static_cast<uint32_t>(remaining_ms_clamped / 1000),
        .remaining_ms = static_cast<uint32_t>(remaining_ms_clamped),
        .monotonic_us = static_cast<uint64_t>(esp_timer_get_time()),
    };

    xQueueOverwrite(snapshot_queue_, &snapshot);
}

void TimerEngine::enqueue_time_delta(const TimeDeltaEvent& event) {
    if (delta_queue_ == nullptr) {
        return;
    }
    xQueueSend(delta_queue_, &event, portMAX_DELAY);
}

}  // namespace dial
