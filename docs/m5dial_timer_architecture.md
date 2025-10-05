# M5 Dial Countdown Timer Architecture & Plan

## 1. Hardware Snapshot

- **Core**: ESP32-S3-WROOM-1U-N16R8 module (dual-core 240 MHz, 16 MB flash, 8 MB PSRAM, Wi-Fi/BLE)
- **Display**: GC9A01 1.32" 240×240 round IPS TFT (Φ32.4 mm active area) on FPC-05F-16PH20 connector, driven over SPI
- **Input**: MT6701CT-STD magnetic angle sensor on shared I2C bus with press button on GPIO5; BOOT strap (IO0) held high via 10 kΩ pull-up
- **Sensing**: Battery monitor on IO4 (`BAT_AD`) via 2×330 kΩ divider to GND
- **Driver**: EG2133 power/backlight management for the round LCD
- **Power chain**: KH-TYPE-C-16P USB-C, TP4054 Li-ion charger, ME6217C33 LDO, MT3608 boost converter
- **Aux peripherals**: Optional DRV8833 haptic pads and speaker amp pads present but unused in firmware; ESP32 IO0 reserved for boot, IO3/7/21/35–40/45–48 unbonded
- **Baseline drivers**: ESP-IDF components (`esp_lcd`, `esp_driver_spi/i2c/ledc`, `gpio`) tailored for the dial board

> Note: We will treat HW inventory abstractly so the firmware builds against `m5dial` board profile without custom pin definitions in-line.

## 2. Technology Stack

| Layer                    | Choice                                             | Rationale                                                                 |
|--------------------------|----------------------------------------------------|---------------------------------------------------------------------------|
| Build system             | **ESP-IDF 5.2** workspace with CMake               | Deterministic builds, best RTOS & timer control, OTA tight integration    |
| Component manager        | `idf-component-manager` (`idf_component.yml`)      | Pulls in `lvgl` and any thin ESP-IDF helpers without Arduino baggage       |
| UI toolkit               | **LVGL 8.3** + custom `esp_lcd` GC9A01 driver      | Stable API, aligned with ESP-IDF examples, zero Arduino dependencies      |
| Concurrency model        | ESP-IDF FreeRTOS tasks + lock-free ring buffers    | Meets latency requirement, isolates UI/IO/timer loops                     |
| Persistence              | ESP-IDF NVS + JSON config overlay (`cJSON`)        | Durable settings, OTA-robust backing                                     |
| Telemetry (P2)           | NimBLE (built-in) + custom GATT/BLE Adv            | Lightweight broadcast; optional compile flag                              |
| Audio                    | I2S out (class-D amp) with sample-based cues       | Non-blocking; amplitude shaped w/ envelope                                |
| Testing                  | Unity + Ceedling (component tests) + host golden   | Automatable, integrates with IDF `idf.py ut`                              |
| Tooling                  | `idf.py` for build/flash/monitor; `pytest` for host validation of state machine | Familiar to ESP developers, scriptable                                   |

### Programming language

Primary implementation in **C++17** (`-std=gnu++17`) for embedded firmware to balance performance & expressiveness. Unit tests and configuration tooling in Python 3.11. Build system + components remain in CMake/ESP-IDF standard.

## 3. Repository Restructure

```
.
├─ apps/
│  └─ m5dial-timer/
│     ├─ CMakeLists.txt               # esp-idf app entry
│     ├─ main/
│     │  ├─ CMakeLists.txt
│     │  └─ app_main.cpp              # boot + supervisor wiring components together
│     ├─ components/
│     │  ├─ board/                    # hardware bring-up (display, touch, power, motor)
│     │  ├─ input/                    # encoder reader + time selector
│     │  ├─ timer/                    # countdown engine + state machine
│     │  ├─ ui/                       # LVGL display driver + root views
│     │  └─ services/                 # persistence, future system services
│     ├─ sdkconfig.defaults
│     ├─ idf_component.yml
│     ├─ test/                         # unity tests & host fuzz
│     └─ docs/
│        ├─ ui_spec.md
│        ├─ config_matrix.md
│        └─ acceptance_checklist.md
├─ tools/
│  ├─ scripts (flash, profiling, soak tests)
│  └─ host-sim (optional python UI simulator)
└─ docs/
   ├─ m5dial_timer_architecture.md (this file)
   └─ roadmap.md
```

Legacy SmartKnob firmware stays under `firmware/` untouched. `apps/` will be the new multi-product workspace; future projects can drop into sibling folders.

## 4. System Architecture

### Board Support Layer

`components/board` now exposes a `DialBoard` facade that keeps the ESP32-S3 alive via
`POWER_HOLD`, drives the GC9A01 display with DMA-backed SPI and LEDC dimming for the
backlight, and initialises the FT3267 touch controller over the shared encoder I2C bus.
The board layer owns lifetime for these peripherals, providing lightweight hooks so UI,
input, and services modules can request refreshes or read touch coordinates without
reimplementing bring-up code.

### Task Topology

- **Encoder Task (core 0, priority 9)** polls the MT6701 over I2C, converts angles into detent ticks, and queues `TimeDeltaEvent`s.
- **Touch Task (core 1, priority 5)** samples the FT3267 controller and emits tap/double-tap/swipe/two-finger gestures for timer control.
- **Motor Task (core 1, priority 6)** drives the EG2133 via LEDC, using MT6701 angle feedback to generate voltage-mode detents.
- **Timer Engine Task (core 0, prio 10)** maintains authoritative countdown state using `esp_timer` (1 ms). Publishes `TimerSnapshot` at 30 Hz or on significant change.
- **UI Task (core 1, prio 8)** hosts LVGL loop at 60 FPS with dirty rectangles + vsync aware double buffering. Receives input and timer snapshots via message queues.
- **Feedback hooks (future)** reserved for optional LED/audio outputs once the hardware is populated.
- **Supervisor (core 0, prio 11)** monitors watchdogs, handles persistence writes, WDT feed, power state transitions (dim/idle/wake).

All tasks communicate through checksumed `struct` messages in a central `event_bus`. Messages stored in `etl::variant` to avoid heap usage in ISR path.

### Data Flow

1. Touch + encoder tasks poll hardware and enqueue events (detent deltas, tap/double-tap/swipe/two-finger gestures).
2. `TimeSelector` converts detents into configurable increments (15 min base with velocity multipliers) and pushes commit/control events.
3. Upon inactivity (1 s), the selector commits the setpoint and transitions to `Countdown` or `Idle` based on auto-start configuration.
4. Timer engine updates high-resolution remaining time; publishes to UI, LED, audio.
5. UI renders 3 layers: background gradient (duration-aware color semantic), adaptive HH:MM:SS / MM:SS readout in a monospaced face, and a 360° progress ring with eased "spring unwind" motion that tracks durations up to 6 h. Color cues follow proportional thresholds (green >10 %, yellow 10–5 %, red ≤5 %) with floor guards at 10 min/5 min (or 2 min/1 min for short timers). Each threshold crossing animates via ≤150 ms fade and adds a non-color cue (ring pulse) for accessibility. LVGL tasks run with `lv_tick_inc(5)` triggered by `esp_timer` every 5 ms.
6. Optional feedback outputs (LED/audio) can subscribe to the same event stream once hardware is added. Haptics reuse the same event stream to provide virtual detents and torque cues.
Default haptic tuning (7 pole pairs, 50 kHz PWM, voltage-mode detents) tracks the Makerfabs MaTouch Knob reference implementation of the EG2133 + MT6701 stack.

### State Machine

Driver-level state machine with states: `Idle`, `Editing`, `Arming`, `Counting`, `Finished`, `Dimmed`. Each state defines allowed transitions, entry/exit actions. Automatic recovery uses persisted snapshot (NVS) storing state, setpoint, remaining ms, timestamp.

### Persistence & Config

- Settings stored under NVS namespace `cfg`: increments (5/15/30/60 min options), max duration (1/2/4/6 h), feedback mode, auto-start, dim timeout.
- Snapshot stored under `state`: last setpoint, remaining ms, state enum, monotonic timestamp.
- On boot, if persisted state indicates active countdown and timestamp delta < max drift threshold (configurable), resume with adjusted remaining time else fail-safe to Idle.

### Timing & Performance Budget

- Encoder ISR: < 10 µs per tick, zero allocations.
- Input Task: 1 ms budget, runs at 1 kHz.
- Timer Engine: uses `esp_timer` callback (1 ms), meter uses monotonic hardware timer, enforces ≤6 h clamp, ensures drift ≤ 1 s/30 min.
- UI: 16 ms frame budget, double buffering ensures <50 ms end-to-end; typical pipeline 20 ms.
- Feedback: LED updates aggregated to 50 Hz with easing curves.

### Watchdog Strategy

- Enable `ESP_TASK_WDT` for core tasks with 2 s timeout.
- Use `esp_restart_pro` to recover after WDT, rehydrate state from NVS in 3 s.

## 5. Feature Mapping (P0 focus)

| Requirement | Plan Highlights |
|-------------|-----------------|
| R1/R2/R3    | 60 FPS LVGL, splitted tasks, dirty rectangles, `esp_timer` countdown |
| R4–R7       | Encoder filter + velocity buckets, virtual detents (15 min base) with tiered acceleration, smoothing, auto clamp |
| R8–R11      | LVGL-based UI with adaptive HH:MM:SS readout, 360° progress ring, eased unwind animation |
| R13         | Duration-scaled color semantics with floor guards, smooth fades, and accessibility pulses |
| R16         | Bounds defined in config, enforced at input & engine level, hard clamp at 6 h |
| R18/R19     | Config store with selectable increments (5/15/30/60 min) and max durations (1–6 h) persisted in NVS |
| R23–R25     | Calibrated esp_timer using RTC for periodic correction; WDT + NVS snapshot |

P1/P2 features layered via compile-time flags and optional tasks (audio, telemetry, OTA).

## 6. Development Workflow

1. `idf.py set-target esp32s3` (persisted per-app)
2. `idf.py menuconfig` (if needed) but defaults set in `sdkconfig.defaults`
3. `idf.py build flash monitor`
4. Unit tests: `idf.py -T input_filter test` (Unity). Host tests: `pytest tools/host-sim/tests`

Convenience wrappers:
- `scripts/m5dial_build.sh [idf.py args...]` to ensure the ESP-IDF environment is sourced before builds.
- `scripts/m5dial_flash.sh [PORT]` to flash + monitor with optional auto-detected serial ports.
- `scripts/host_sim.sh run` to build and launch the LVGL SDL host simulator.

CI to be set up with GitHub Actions using `espressif/idf:latest` docker, caching `~/.espressif`

## 7. Next Implementation Milestones

1. **Scaffold**: Create app skeleton, stand up the ESP-IDF board layer + LVGL, bring up display + encoder baseline.
2. **Core Loop**: Implement timer engine, input filtering, state machine, NVS settings.
3. **UI**: Build LVGL layout, progress ring, color semantics, highlight feedback.
4. **Feedback**: LED/audio outputs (future work when hardware is populated).
5. **Reliability**: Add WDT, snapshot recovery, drift calibration, soak test scripts.
6. **Polish & Extensibility**: Config screens, OTA, telemetry.
