#include <SDL.h>
#include <lvgl.h>

#include "esp_log.h"
#include "sdl_driver.h"
#include "timer/timer_types.h"
#include "ui/ui_root.h"

namespace {

constexpr int kScreenSize = 240;
constexpr int kFrameIntervalMs = 16;  // ~60 FPS
constexpr uint32_t kDemoSetpointSeconds = 15 * 60;  // demo loop

void update_snapshot(dial::TimerSnapshot& snapshot, uint32_t elapsed_ms) {
    const uint32_t total_ms = kDemoSetpointSeconds * 1000;
    if (elapsed_ms >= total_ms) {
        snapshot.state = dial::TimerState::Finished;
        snapshot.remaining_seconds = 0;
        snapshot.remaining_ms = 0;
        return;
    }

    const uint32_t remaining_ms = total_ms - elapsed_ms;
    snapshot.state = dial::TimerState::Counting;
    snapshot.remaining_ms = remaining_ms;
    snapshot.remaining_seconds = (remaining_ms + 999) / 1000;
}

}  // namespace

int main() {
    if (!host_sim::init(kScreenSize, kScreenSize)) {
        return -1;
    }

    lv_init();
    host_sim::register_display(kScreenSize, kScreenSize);
    host_sim::register_pointer();

    dial::UiConfig ui_cfg{
        .screen_width = static_cast<uint16_t>(kScreenSize),
        .screen_height = static_cast<uint16_t>(kScreenSize),
    };

    if (dial::g_ui_root.init(ui_cfg) != ESP_OK) {
        ESP_LOGE("HostSim", "Failed to initialise UI root");
        host_sim::shutdown();
        return -1;
    }

    dial::TimerSnapshot snapshot{};
    snapshot.state = dial::TimerState::Counting;
    snapshot.setpoint_seconds = kDemoSetpointSeconds;
    snapshot.remaining_seconds = kDemoSetpointSeconds;
    snapshot.remaining_ms = snapshot.remaining_seconds * 1000;

    uint32_t start_ms = SDL_GetTicks();
    uint32_t last_tick_ms = start_ms;
    bool quit = false;

    while (!quit) {
        const uint32_t now_ms = SDL_GetTicks();
        const uint32_t elapsed_ms = now_ms - start_ms;
        update_snapshot(snapshot, elapsed_ms);
        snapshot.monotonic_us = static_cast<uint64_t>(now_ms) * 1000ULL;

        dial::g_ui_root.update(snapshot);

        const uint32_t tick_delta = now_ms - last_tick_ms;
        lv_tick_inc(tick_delta);
        last_tick_ms = now_ms;

        lv_timer_handler();

        host_sim::pump_events(quit);
        host_sim::delay(kFrameIntervalMs);

        if (snapshot.state == dial::TimerState::Finished && !quit) {
            if (elapsed_ms >= (kDemoSetpointSeconds + 5) * 1000) {
                start_ms = SDL_GetTicks();
                last_tick_ms = start_ms;
                snapshot.state = dial::TimerState::Counting;
                snapshot.setpoint_seconds = kDemoSetpointSeconds;
                snapshot.remaining_seconds = kDemoSetpointSeconds;
                snapshot.remaining_ms = snapshot.remaining_seconds * 1000;
            }
        }
    }

    host_sim::shutdown();
    return 0;
}
