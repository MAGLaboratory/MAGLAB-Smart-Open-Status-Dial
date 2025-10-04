#include "timer/state_machine.h"

namespace dial {

TimerState determine_next_state(TimerState current, const TimeDeltaEvent& event, bool auto_start) {
    if (event.type == TimeEventType::Commit) {
        if (event.total_seconds == 0) {
            return TimerState::Idle;
        }
        if (!auto_start) {
            return TimerState::Arming;
        }
        return TimerState::Counting;
    }

    switch (current) {
        case TimerState::Idle:
            if (event.delta_seconds != 0 && event.total_seconds > 0) {
                return TimerState::Editing;
            }
            break;
        case TimerState::Editing:
        case TimerState::Arming:
            if (event.total_seconds == 0) {
                return TimerState::Idle;
            }
            return TimerState::Editing;
        case TimerState::Counting:
            if (event.total_seconds == 0) {
                return TimerState::Finished;
            }
            return TimerState::Editing;  // knob movement interrupts countdown
        case TimerState::Finished:
            if (event.total_seconds > 0) {
                return TimerState::Editing;
            }
            break;
    }
    return current;
}

}  // namespace dial
