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

The simulator opens a 360×360 window and loops a 15-minute countdown. Mouse
input is mapped to LVGL's pointer device for interactive testing.

### Tweaks

- Update `kDemoSetpointSeconds` in `src/sim_main.cpp` to change the run length.
- Replace the synthetic countdown with scripted snapshots once the timer engine
  is host-portable.
