#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

#include "input/encoder_reader.h"

namespace dial {

enum class TimeEventType : uint8_t {
    Delta,
    Commit,
    Control,
};

enum class ControlCommand : uint8_t {
    None,
    ToggleRun,
    Reset,
};

struct TimeSelectorConfig {
    uint32_t base_step_seconds = 15 * 60;   // default 15 minutes
    uint32_t medium_multiplier = 2;         // medium speed multiplier
    uint32_t fast_multiplier = 4;           // fast speed multiplier (1h when base=15m)
    float medium_threshold_tps = 20.0f;     // ticks per second threshold for medium speed
    float fast_threshold_tps = 40.0f;       // ticks per second threshold for fast speed
    uint32_t max_total_seconds = 6 * 3600;  // 6 hours default clamp
    uint32_t queue_depth = 16;
    uint32_t commit_timeout_ms = 1000;      // inactivity window before commit
};

struct TimeDeltaEvent {
    TimeEventType type = TimeEventType::Delta;
    int32_t total_seconds;    // clamped total setpoint after applying delta
    int32_t delta_seconds;    // signed delta applied for this event
    uint32_t timestamp_us;    // when the encoder event occurred
    uint32_t multiplier;      // multiplier applied (1, medium, fast)
    ControlCommand control = ControlCommand::None;
};

class TimeSelector {
public:
    TimeSelector() = default;
    esp_err_t init(const TimeSelectorConfig& config);
    void start();

    QueueHandle_t event_queue() const { return event_queue_; }

private:
    static void task_entry(void* arg);
    void run();
    void process_sample(const EncoderSample& sample);

    TimeSelectorConfig config_{};
    QueueHandle_t event_queue_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    int32_t accumulated_seconds_ = 15 * 60;  // start with default 15 min
    uint32_t last_timestamp_us_ = 0;
    uint64_t last_activity_us_ = 0;
    bool commit_sent_ = false;
};

extern TimeSelector g_time_selector;

}  // namespace dial
