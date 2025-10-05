# M5 Dial LVGL Host Simulator

Desktop harness for the M5 Dial UI using LVGL's SDL backend.

## Prerequisites

- CMake 3.20+
- C++17 toolchain (clang/gcc)
- SDL2 development package (`libsdl2-dev`, `brew install sdl2`, etc.)

## Build & Run

```
cmake -S tools/host-sim -B build/host-sim
cmake --build build/host-sim
./build/host-sim/m5dial_host_sim
```

The simulator opens a 240×240 window and loops a 15-minute countdown by default.
Mouse input is mapped to LVGL's pointer device for interactive testing.

To replay recorded snapshots from hardware, pass a snapshot log path plus an optional modifier log:

```
./build/host-sim/m5dial_host_sim snapshots.txt [modifiers.txt]
```

Snapshot log format: each line `monotonic_us state setpoint_seconds remaining_ms` (state uses the `dial::TimerState` enum value). Lines starting with `#` are ignored.

Modifier log format (optional): `monotonic_us type value`. Use `C <control>` for gesture commands (integer cast of `dial::ControlCommand`) or `D <delta_seconds>` for timer deltas.

The simulator loops both logs after a short hold at the end.

### Tweaks

- Update `kDemoSetpointSeconds` in `src/sim_main.cpp` to change the synthetic run length.
- Adjust the replay loop or add new event handling in `src/sim_main.cpp` as needed.
