# MaTouch Knob Reference Notes

This summarizes the Makerfabs MaTouch Knob firmware and highlights pieces we can reuse or adapt for the M5 Dial timer project.

## Repository Snapshot

- Location: `thirdparty/MaTouch_Knob`
- Primary example: `example/matouch_knob/matouch_knob.ino`
- Key modules:
  - `display_task.h`: LVGL + Arduino_GFX setup for GC9A01 round TFT
  - `touch.{h,cpp}`: I²C driver for the FT6X36 capacitive touch IC
  - `motor_task.h`: SimpleFOC haptic motor driver (not present on M5 Dial)
  - `interface.h`: Input events, USB HID/BLE keyboard integration, power + watchdog helpers
  - `ui*.c`: LVGL assets generated with SquareLine Studio

The firmware targets ESP32-S3 with Arduino core and FreeRTOS tasks pinned per function (motor, interface, display).

## Useful Concepts for M5 Dial Timer

- **LVGL bring-up**: `display_task.h:33` shows GC9A01 init, double buffering with PSRAM, and LVGL registration. We can port the buffering pattern into our ESP-IDF `ui` component, replacing Arduino_GFX with LovyanGFX (already bundled in M5Unified).
- **Touch controller**: `touch.cpp:5` implements raw FT6X36 reads. M5 Dial also uses FT6336U; the register map aligns, so we can lift the transaction format while swapping to ESP-IDF I²C APIs.
- **Task separation**: The example pins interface, motor, and display loops to specific cores (`matouch_knob.ino:31`). This reinforces our architecture choice to isolate UI and input tasks.
- **Asset pipeline**: SquareLine-generated LVGL assets live in `ui*.c`. We can reuse the pattern (generate `ui.h`, etc.) for our custom visuals once we design screens in SquareLine/LVGL.
- **Watchdog + web config**: `Watchdog.h`, `WifiAsyncWebServer.*` provide simple wrappers for OTA and remote config. These may accelerate P1/P2 features (OTA, diagnostics) after we translate them from Arduino to ESP-IDF equivalents.

## Caution / Non-Reusable Pieces

- **SimpleFOC haptics** (`motor_task.h`): Relies on BLDC driver and motor hardware absent on M5 Dial; omit.
- **Arduino-only APIs**: The sample leans on Arduino (`Wire`, `SPIClass`, `AsyncWebServer`). We must reimplement using ESP-IDF drivers or wrap the Arduino layer via `idf-component` if we decide to enable dual runtime.
- **Global state**: Uses globals and naked queues (`queue_`, `knob_state_queue_`). Our implementation should encapsulate these into typed message buses.
- **Timing assumptions**: `display_run` calls `lv_timer_handler()` every 10 ms without explicit tick management; we already plan a deterministic LVGL tick via `esp_timer`.

## Integration Plan

1. Extract touch read logic into `apps/m5dial-timer/components/input/` (a dedicated FT3267 controller) using ESP-IDF I²C.
2. Mirror the double-buffer allocation strategy (PSRAM-backed) inside our `components/ui/src/display_driver.cpp`, substituting LovyanGFX.
3. Reference the task structuring and queue usage when defining our FreeRTOS message bus.
4. Optionally port the USB HID dial report descriptor for a future HID control mode.

For now only the hardware assets from the Makerfabs repo are vendored under `thirdparty/MaTouch_Knob`. The Arduino examples and libraries were dropped after translating their pin definitions into `PinMap` (see `docs/m5dial_pinmap.md`). We will cherry-pick concepts rather than import code wholesale to keep the new firmware clean and IDF-native.
