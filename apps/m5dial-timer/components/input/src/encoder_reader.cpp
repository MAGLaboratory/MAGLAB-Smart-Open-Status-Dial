#include "input/encoder_reader.h"

#include <driver/gpio.h>
#include <esp_attr.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <hal/gpio_ll.h>

#include <atomic>

namespace dial {

namespace {
constexpr const char* TAG = "EncoderReader";

std::atomic<int32_t> g_prev_state{0};
QueueHandle_t g_sample_queue = nullptr;

inline int32_t read_encoder_state(uint32_t gpio_a, uint32_t gpio_b) {
    const int a = gpio_ll_get_level(&GPIO, gpio_a);
    const int b = gpio_ll_get_level(&GPIO, gpio_b);
    return (a << 1) | b;
}

void IRAM_ATTR encoder_isr(void* arg) {
    auto cfg = static_cast<EncoderConfig*>(arg);
    const int32_t prev = g_prev_state.load(std::memory_order_relaxed);
    const int32_t current = read_encoder_state(cfg->gpio_a, cfg->gpio_b);

    static const int8_t trans_table[16] = {
        0, -1, +1,  0,
        +1, 0,  0, -1,
        -1, 0,  0, +1,
         0, +1, -1, 0,
    };

    const int8_t delta = trans_table[(prev << 2) | current];
    if (delta != 0) {
        const int32_t adjusted = cfg->invert ? -delta : delta;
        EncoderSample sample{.delta_ticks = adjusted, .timestamp_us = (uint32_t)esp_timer_get_time()};
        if (g_sample_queue != nullptr) {
            BaseType_t hp_task_woken = pdFALSE;
            xQueueSendFromISR(g_sample_queue, &sample, &hp_task_woken);
            if (hp_task_woken == pdTRUE) {
                portYIELD_FROM_ISR();
            }
        }
    }
    g_prev_state.store(current, std::memory_order_relaxed);
}

}  // namespace

EncoderReader g_encoder_reader;

esp_err_t EncoderReader::init(const EncoderConfig& config) {
    config_ = config;

    if (static_cast<int32_t>(config.gpio_a) < 0 || static_cast<int32_t>(config.gpio_b) < 0) {
        ESP_LOGW(TAG, "Quadrature encoder pins not configured; MT6701 driver pending");
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (sample_queue_ == nullptr) {
        sample_queue_ = xQueueCreate(config.sample_queue_depth, sizeof(EncoderSample));
        if (sample_queue_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create encoder queue");
            return ESP_ERR_NO_MEM;
        }
        g_sample_queue = sample_queue_;
    }

    const esp_err_t isr_status = gpio_install_isr_service(0);
    if (isr_status != ESP_OK && isr_status != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(isr_status));
        return isr_status;
    }

    gpio_config_t io_conf{};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = (1ULL << config.gpio_a) | (1ULL << config.gpio_b);
    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "gpio_config failed");

    const int32_t initial_state = read_encoder_state(config.gpio_a, config.gpio_b);
    g_prev_state.store(initial_state, std::memory_order_relaxed);

    ESP_RETURN_ON_ERROR(gpio_isr_handler_add((gpio_num_t)config.gpio_a, encoder_isr, (void*)&config_), TAG, "Failed to add ISR A");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add((gpio_num_t)config.gpio_b, encoder_isr, (void*)&config_), TAG, "Failed to add ISR B");

    ESP_LOGI(TAG, "Encoder reader initialised (A=%u, B=%u)", config.gpio_a, config.gpio_b);
    return ESP_OK;
}

}  // namespace dial
