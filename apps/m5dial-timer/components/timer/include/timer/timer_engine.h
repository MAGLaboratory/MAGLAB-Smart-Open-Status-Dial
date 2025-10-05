#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_timer.h"

#include "input/time_selector.h"
#include "timer/timer_types.h"

namespace dial {

struct TimerEngineConfig {
    uint32_t max_total_seconds = 6 * 3600;
    uint32_t snapshot_hz = 30;
    bool auto_start = true;
};

class TimerEngine {
public:
    TimerEngine() = default;
    esp_err_t init(const TimerEngineConfig& config);
    void start();

    QueueHandle_t snapshot_queue() const { return snapshot_queue_; }
    void enqueue_time_delta(const TimeDeltaEvent& event);
    void enqueue_control(ControlCommand command);
    void enqueue_quick_delta(int32_t delta_seconds);

private:
    static void timer_callback(void* arg);
    static void task_entry(void* arg);

    void run();
    void publish_snapshot();
    void on_tick();

    TimerEngineConfig config_{};
    esp_timer_handle_t esp_timer_ = nullptr;
    QueueHandle_t snapshot_queue_ = nullptr;
    QueueHandle_t delta_queue_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;

    TimerState state_ = TimerState::Idle;
    uint32_t setpoint_seconds_ = 15 * 60;
    int64_t remaining_ms_ = static_cast<int64_t>(15 * 60 * 1000);
};

extern TimerEngine g_timer_engine;

}  // namespace dial
