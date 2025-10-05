#pragma once

#include <cstdint>
#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

namespace dial {

struct HapticsConfig {
    int gpio_u = 15;
    int gpio_v = 16;
    int gpio_w = 17;
    uint32_t pwm_frequency_hz = 50000;
    uint32_t resolution_bits = 12;
    uint8_t pole_pairs = 7;
    uint16_t detent_positions = 96;
    float detent_strength = 0.6f;
    float max_voltage_ratio = 0.4f;
    TickType_t update_interval_ticks = pdMS_TO_TICKS(1);
};

class MotorController {
public:
    MotorController() = default;

    esp_err_t init(const HapticsConfig& config);
    void start();
    void enable(bool enabled);
    void set_strength(float strength);

private:
    static void task_entry(void* arg);
    void run();
    void apply_pwm(float a, float b, float c);

    HapticsConfig config_{};
    bool initialised_ = false;
    TaskHandle_t task_handle_ = nullptr;
    std::atomic<bool> enabled_{true};
    std::atomic<float> strength_scale_{1.0f};
    float electrical_offset_ = 0.0f;
};

extern MotorController g_motor_controller;

}  // namespace dial
