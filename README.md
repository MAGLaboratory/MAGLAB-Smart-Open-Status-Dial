# MAGLAB Smart Open Status Dial

Firmware and tooling for the MAGLAB Smart Open Status Dial, a countdown/status display built on the M5Stack Dial
(ESP32-S3). This fork of [scottbez1/smartknob](https://github.com/scottbez1/smartknob) focuses on turnkey firmware for
status indication rather than the original haptic research platform.

> Looking for the upstream SmartKnob project? Head to the original repository for torque-feedback research hardware,
> mechanical CAD, and community resources. This fork narrows in on the M5 Dial application firmware.

## Why this fork exists

- Dedicated application: polished countdown/status UX for the M5Stack Dial without additional hardware tweaks.
- ESP-IDF based firmware scaffold under `apps/m5dial-timer/` with modular components (board, input, timer, UI, services).
- Desktop LVGL simulator and helper scripts for quicker iteration.
- Documentation tailored to the timer workflow (architecture, roadmap, pin map, MaTouch reference).

## Hardware snapshot

| Component | Notes |
|-----------|-------|
| M5Stack Dial | ESP32-S3, GC9A01 360×360 round display, FT3267 touch overlay, MT6701 angle sensor |
| Power | Built-in AXP2101 PMIC controls; firmware keeps `POWER_HOLD` asserted |
| Inputs | MT6701 magnetic angle sensor (I2C), push button on GPIO5 |
| Outputs | GC9A01 display over SPI with LEDC backlight, placeholder RGB LED data pin |
| Optional | DRV8833 motor pads, speaker amplifier pads (not yet driven by firmware) |

Full pin assignments live in `docs/m5dial_pinmap.md` and are auto-generated from `board/pinmap.h`.

## Firmware highlights

- High-resolution countdown engine using `esp_timer` with NVS-backed state persistence.
- LVGL 8 UI with progress arc, adaptive HH:MM[:SS] readout, and color semantics for status ranges.
- Board support layer that initializes display, touch, power latch, and backlight dimming.
- Input pipeline scaffold prepared for MT6701 sampling; legacy quadrature paths stubbed for testing.
- Configurable helper scripts for build, flash, and desktop simulation.

## Repository guide

| Path | Description |
|------|-------------|
| `apps/m5dial-timer/` | ESP-IDF application entry point and components |
| `apps/m5dial-timer/components/board/` | Dial hardware bring-up (display, touch, power, motor placeholders) |
| `apps/m5dial-timer/components/input/` | Encoder reader scaffolding and time selector logic |
| `apps/m5dial-timer/components/timer/` | Countdown engine, state machine, and data types |
| `apps/m5dial-timer/components/ui/` | LVGL driver glue and UI root widgets |
| `apps/m5dial-timer/components/services/` | Shared services such as NVS persistence |
| `tools/host-sim/` | SDL-based LVGL simulator for desktop testing |
| `scripts/` | Helper scripts (`m5dial_build.sh`, `m5dial_flash.sh`, `host_sim.sh`, `doc_sync.sh`) |
| `docs/` | Architecture overview, roadmap, pin map, MaTouch reference |
| `thirdparty/MaTouch_Knob/` | Reference hardware documentation from MaTouch |

## Build & flash

Prerequisites: ESP-IDF 5.2 (clone/export via Espressif installer) and a connected M5Stack Dial.

```
# Set up ESP-IDF environment and build the firmware
scripts/m5dial_build.sh set-target esp32s3
scripts/m5dial_build.sh build

# Flash and monitor (auto-detects serial port if not provided)
scripts/m5dial_flash.sh /dev/ttyACM0
```

Advanced usage: call `idf.py` directly inside `apps/m5dial-timer/` once your ESP-IDF environment is active. The helper
scripts ensure the environment is sourced correctly and mirror the commands above.

## Host simulator

Iterate on the LVGL UI without hardware using the SDL host simulator:

```
scripts/host_sim.sh run
```

A 360×360 window emulates the dial display and plays through a sample countdown sequence. Modify `tools/host-sim/src`
to add scripted scenarios or integrate with host-side tests.

## Documentation hub

- `docs/m5dial_timer_architecture.md` — system architecture, task topology, and tooling workflow.
- `docs/m5dial_timer_roadmap.md` — implementation checklist with current status.
- `docs/m5dial_pinmap.md` — auto-generated pin mapping sourced from `PinMap` constants.
- `docs/matouch_knob_reference.md` — vendor-provided reference material for the base hardware.

## Contributing & licensing

Contributions are welcome via pull requests. Coordinate with upstream if you intend to merge changes back into
scottbez1/smartknob; this fork tracks upstream through the `origin` remote while `maglab` hosts the MAGLAB-specific
work.

License information is inherited from the upstream project (see `LICENSE`). Please respect the original authorship when
sharing derivative works or documentation.
