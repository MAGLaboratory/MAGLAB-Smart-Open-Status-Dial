# M5 Dial Countdown Timer Firmware

This application targets the M5Stack Dial (ESP32-S3) and implements a polished countdown timer UX.

## Current Status

- `DialBoard` initialises power hold, GC9A01 display + LEDC backlight, and the FT3267 touch controller.
- Timer engine runs at 1 ms resolution, feeds LVGL snapshots, and persists state in NVS.
- Baseline LVGL UI draws a progress arc and adaptive HH:MM[:SS] readout; host SDL simulator mirrors the layout.

## Build & Flash

Ensure ESP-IDF 5.2 is installed and exported (the helper script sources it automatically):

```
scripts/m5dial_build.sh set-target esp32s3
scripts/m5dial_build.sh build
scripts/m5dial_flash.sh /dev/ttyACM0   # omit the port to auto-detect
```

You can still invoke `idf.py` directly from `apps/m5dial-timer/` if you already have the environment active.

## Host Simulator

An SDL-based LVGL harness lives under `tools/host-sim` for UI iteration without hardware:

```
scripts/host_sim.sh run
```

The simulator launches a 360×360 window that follows the same snapshot updates the firmware publishes.

## Further Reading

- `docs/m5dial_timer_architecture.md` – high-level architecture and workflow.
- `docs/m5dial_timer_roadmap.md` – roadmap with current progress.
