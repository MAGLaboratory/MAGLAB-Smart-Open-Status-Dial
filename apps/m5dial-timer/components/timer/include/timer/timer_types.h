#pragma once

#include <cstdint>

namespace dial {

enum class TimerState : uint8_t {
    Idle,
    Editing,
    Arming,
    Counting,
    Finished,
};

struct TimerSnapshot {
    TimerState state = TimerState::Idle;
    uint32_t setpoint_seconds = 0;
    uint32_t remaining_seconds = 0;
    uint32_t remaining_ms = 0;
    uint64_t monotonic_us = 0;
};

}  // namespace dial
