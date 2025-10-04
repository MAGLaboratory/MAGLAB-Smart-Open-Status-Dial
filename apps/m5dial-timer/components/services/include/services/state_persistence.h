#pragma once

#include <cstdint>

#include <esp_err.h>

#include "timer/timer_engine.h"

namespace dial::persistence {

struct RestoredState {
    TimerState state = TimerState::Idle;
    uint32_t setpoint_seconds = 0;
    uint32_t remaining_ms = 0;
    bool valid = false;
};

esp_err_t init();
esp_err_t save(const TimerSnapshot& snapshot);
esp_err_t load(RestoredState* out);

}  // namespace dial::persistence
