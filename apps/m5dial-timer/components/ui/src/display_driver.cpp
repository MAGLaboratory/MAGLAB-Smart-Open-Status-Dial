#include "ui/display_driver.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lvgl.h>

#include "board/dial_board.h"

namespace dial {

namespace {
constexpr const char* TAG = "DisplayDriver";
constexpr uint32_t kLvTickMs = 5;

lv_disp_draw_buf_t draw_buf;
lv_color_t* buf1 = nullptr;
lv_color_t* buf2 = nullptr;
lv_disp_drv_t disp_drv;
esp_timer_handle_t tick_timer = nullptr;
SemaphoreHandle_t lvgl_mutex = nullptr;
TaskHandle_t lvgl_task_handle = nullptr;
bool initialised = false;

void IRAM_ATTR lv_tick_cb(void* /*arg*/) {
    lv_tick_inc(kLvTickMs);
}

void lvgl_task(void* /*arg*/) {
    while (true) {
        if (lvgl_mutex != nullptr && xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGiveRecursive(lvgl_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(kLvTickMs));
    }
}

void disp_flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    if (!g_board.config().enable_display) {
        lv_disp_flush_ready(disp);
        return;
    }

    DisplayRegion region{
        .x = area->x1,
        .y = area->y1,
        .width = area->x2 - area->x1 + 1,
        .height = area->y2 - area->y1 + 1,
    };

    const esp_err_t err = g_board.display().flush(region, reinterpret_cast<const uint16_t*>(color_p));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Display flush failed: %s", esp_err_to_name(err));
    }

    lv_disp_flush_ready(disp);
}

}  // namespace

esp_err_t init_lvgl_display() {
    if (initialised) {
        return ESP_OK;
    }

    if (!g_board.config().enable_display) {
        ESP_LOGE(TAG, "Display not enabled in board config");
        return ESP_ERR_INVALID_STATE;
    }

    lv_init();

    lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (lvgl_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create LVGL mutex");
        return ESP_ERR_NO_MEM;
    }

    const int32_t width = g_board.display().width();
    const int32_t height = g_board.display().height();
    const size_t buf_pixels = static_cast<size_t>(width) * static_cast<size_t>(height);

    buf1 = static_cast<lv_color_t*>(heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM));
    buf2 = static_cast<lv_color_t*>(heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM));
    if (buf1 == nullptr || buf2 == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate LVGL frame buffers");
        return ESP_ERR_NO_MEM;
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_pixels);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = width;
    disp_drv.ver_res = height;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    esp_timer_create_args_t tick_args{
        .callback = &lv_tick_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick",
        .skip_unhandled_events = true,
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &tick_timer), TAG, "Failed to create tick timer");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(tick_timer, kLvTickMs * 1000), TAG, "Failed to start tick timer");

    const BaseType_t res = xTaskCreatePinnedToCore(lvgl_task, "lvgl", 4096, nullptr, 5, &lvgl_task_handle, 1);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        return ESP_FAIL;
    }

    initialised = true;
    ESP_LOGI(TAG, "LVGL display initialised (%d x %d)", width, height);
    return ESP_OK;
}

void lvgl_acquire() {
    if (lvgl_mutex) {
        xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
    }
}

void lvgl_release() {
    if (lvgl_mutex) {
        xSemaphoreGiveRecursive(lvgl_mutex);
    }
}

}  // namespace dial
