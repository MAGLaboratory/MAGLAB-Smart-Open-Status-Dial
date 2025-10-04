#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "board/dial_board.h"
#include "board/pinmap.h"
#include "input/encoder_reader.h"
#include "input/time_selector.h"
#include "timer/timer_engine.h"
#include "ui/display_driver.h"
#include "ui/ui_root.h"
#include "services/state_persistence.h"

namespace {
constexpr const char* TAG = "app_main";

void time_event_dispatch(void* arg) {
    (void)arg;
    dial::TimeDeltaEvent event;
    while (true) {
        if (xQueueReceive(dial::g_time_selector.event_queue(), &event, portMAX_DELAY) == pdTRUE) {
            dial::g_timer_engine.enqueue_time_delta(event);
        }
    }
}

void ui_dispatch_task(void* arg) {
    (void)arg;
    dial::TimerSnapshot snapshot;
    while (true) {
        if (xQueueReceive(dial::g_timer_engine.snapshot_queue(), &snapshot, portMAX_DELAY) == pdTRUE) {
            dial::lvgl_acquire();
            dial::g_ui_root.update(snapshot);
            dial::lvgl_release();
        }
    }
}
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "M5 Dial timer firmware scaffold booting");

    esp_err_t nvs_status = nvs_flash_init();
    if (nvs_status == ESP_ERR_NVS_NO_FREE_PAGES || nvs_status == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_status = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_status);

    ESP_ERROR_CHECK(dial::persistence::init());

    const dial::DialBoardConfig board_cfg{};
    ESP_ERROR_CHECK(dial::g_board.init(board_cfg));

    const dial::EncoderConfig encoder_cfg{
        .gpio_a = dial::PinMap::ENCODER_A,
        .gpio_b = dial::PinMap::ENCODER_B,
        .invert = false,
        .sample_queue_depth = 128,
    };
    esp_err_t encoder_status = dial::g_encoder_reader.init(encoder_cfg);
    if (encoder_status == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Quadrature encoder not configured; MT6701 reader pending");
    } else {
        ESP_ERROR_CHECK(encoder_status);
    }

    const dial::TimeSelectorConfig selector_cfg{};
    ESP_ERROR_CHECK(dial::g_time_selector.init(selector_cfg));
    dial::g_time_selector.start();

    const dial::TimerEngineConfig timer_cfg{};
    ESP_ERROR_CHECK(dial::g_timer_engine.init(timer_cfg));
    dial::g_timer_engine.start();

    ESP_ERROR_CHECK(dial::init_lvgl_display());
    dial::lvgl_acquire();
    ESP_ERROR_CHECK(dial::g_ui_root.init({}));
    dial::lvgl_release();

    dial::TimerSnapshot initial_snapshot{};
    if (xQueuePeek(dial::g_timer_engine.snapshot_queue(), &initial_snapshot, 0) == pdTRUE) {
        dial::lvgl_acquire();
        dial::g_ui_root.update(initial_snapshot);
        dial::lvgl_release();
    }

    xTaskCreatePinnedToCore(&time_event_dispatch, "time_evt", 4096, nullptr, 5, nullptr, 0);
    xTaskCreatePinnedToCore(&ui_dispatch_task, "ui_evt", 4096, nullptr, 5, nullptr, 1);

    while (true) {
        dial::g_board.update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
