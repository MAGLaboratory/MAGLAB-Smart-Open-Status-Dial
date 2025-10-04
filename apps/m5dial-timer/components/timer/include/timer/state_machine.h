#pragma once

#include "timer/timer_engine.h"

namespace dial {

TimerState determine_next_state(TimerState current, const TimeDeltaEvent& event, bool auto_start);

}  // namespace dial
