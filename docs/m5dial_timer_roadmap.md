# M5 Dial Timer Roadmap

## Phase 0 – Foundation & Bring-up (P0 core)

- [x] **Toolchain setup**
  - ESP-IDF 5.2 builds verified via `idf.py set-target esp32s3`.
  - Helper scripts (`scripts/m5dial_build.sh`, `scripts/m5dial_flash.sh`) source the toolchain locally; CI workflow still planned.
- [ ] **Hardware abstraction layer**
  - `DialBoard` powers the GC9A01 display, LEDC backlight, and FT3267 touch controller.
  - MT6701 angle sensor and motor driver wiring remain TODO; PMIC hooks still stubbed.
- [ ] **Input pipeline** (R4–R7)
  - Quadrature ISR + sample queue scaffolded; MT6701 reader and velocity buckets pending.
  - Snap-to-detent visuals follow once accelerated input lands.
- [x] **Timer engine** (R1/R3/R16/R23)
  - `TimerEngine` runs a 1 ms `esp_timer`, clamps to a configurable max, and persists snapshots via NVS.
  - Idle, Editing, Counting, and Finished transitions handled through the shared state machine helpers.
- [x] **UI baseline** (R1/R2/R8–R11)
  - LVGL root renders arc progress plus adaptive HH:MM[:SS] readout with color semantics.
  - Host SDL simulator (`scripts/host_sim.sh run`) available for quick UI iteration.
- [ ] **Reliability** (R23–R25)
  - State persistence landed; watchdog wiring and brownout/power recovery remain open.

## Phase 1 – Polish (P1)

1. **Feedback** (R12–R14)
   - LED ring cues, audio tick + chime with volume control.
2. **Config layer** (R18–R22)
   - Settings UI or JSON import via USB serial.
   - Auto-start mode, increment presets (5/15/30/60 min), max duration (1/2/4/6 h), dim/idle timer, audio toggles.
3. **Visual refinements** (R26–R28)
   - Typography assets, easing curves, design review checklist.
4. **Power/idle behavior**
   - Backlight dimming, touch/encoder wake, ambient light integration (optional).

## Phase 2 – Integrations (P2)

1. **BLE telemetry** (R29)
   - Advertising packet with remaining time, optional GATT service.
2. **OTA pipeline** (R30)
   - Secure OTA via HTTPS; fallback partition scheme.
3. **Diagnostics** (R31)
   - Hidden diagnostics panel, logging categories, stats.

## Testing Strategy

- **Latency harness**: instrument ISR → UI commit using GPIO toggle + logic analyzer; integrate with `idf_monitor` markers.
- **Drift test**: continuous 8 h run vs calibrated RTC, log drift.
- **Abuse tests**: automated `encoder_spin` script (M5Stack motor or host simulation) for rapid range sweeps, start/stop spam.
- **Usability script**: host-sim script to randomize targets and log attempts/time-to-set for hallway testing.

## Deliverables Snapshot

- Firmware under `apps/m5dial-timer/`.
- Generated LVGL assets + style guide doc.
- Config schema (JSON + NVS mapping).
- Installer instructions (idf.py, OTA update notes).
- Acceptance checklist derived from requirements.
