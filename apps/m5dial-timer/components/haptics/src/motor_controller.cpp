#include "haptics/motor_controller.h"

#include <algorithm>
#include <cmath>
#include <atomic>

#include <esp_log.h>
#include <esp_timer.h>
#include <driver/ledc.h>

#include "input/encoder_reader.h"

namespace dial {

namespace {
constexpr const char* TAG = "MotorController";
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kPhaseShift = 2.0f * kPi / 3.0f;  // 120 deg

const ledc_channel_t kChannels[3] = {LEDC_CHANNEL_4, LEDC_CHANNEL_5, LEDC_CHANNEL_6};
}

MotorController g_motor_controller;

esp_err_t MotorController::init(const HapticsConfig& config) {
    if (initialised_) {
        return ESP_OK;
    }

    config_ = config;

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = static_cast<ledc_timer_bit_t>(config_.resolution_bits),
        .timer_num = LEDC_TIMER_1,
        .freq_hz = config_.pwm_frequency_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "timer config failed");

    const int gpios[3] = {config_.gpio_u, config_.gpio_v, config_.gpio_w};

    for (int i = 0; i < 3; ++i) {
        ledc_channel_config_t channel_cfg = {};
        channel_cfg.gpio_num = gpios[i];
        channel_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
        channel_cfg.channel = kChannels[i];
        channel_cfg.intr_type = LEDC_INTR_DISABLE;
        channel_cfg.timer_sel = LEDC_TIMER_1;
        channel_cfg.duty = (1u << config_.resolution_bits) / 2;
        channel_cfg.hpoint = 0;
        ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_cfg), TAG, "channel config failed");
    }

    electrical_offset_ = config_.zero_electrical_offset;
    initialised_ = true;
    return ESP_OK;
}

void MotorController::start() {
    if (!initialised_ || task_handle_ != nullptr) {
        return;
    }

    BaseType_t res = xTaskCreatePinnedToCore(
        &MotorController::task_entry,
        "motor_ctrl",
        4096,
        this,
        6,
        &task_handle_,
        1);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create motor control task");
        task_handle_ = nullptr;
    }
}

void MotorController::enable(bool enabled) {
    enabled_.store(enabled, std::memory_order_relaxed);
}

void MotorController::set_strength(float strength) {
    strength_scale_.store(std::clamp(strength, 0.0f, 1.0f), std::memory_order_relaxed);
}

void MotorController::task_entry(void* arg) {
    auto* self = static_cast<MotorController*>(arg);
    self->run();
}

void MotorController::run() {
    const TickType_t delay_ticks = config_.update_interval_ticks;
    const float max_ratio = std::clamp(config_.max_voltage_ratio, 0.0f, 0.49f);

    while (true) {
        uint16_t raw_angle = 0;
        if (!g_encoder_reader.latest_raw_angle(&raw_angle)) {
            apply_pwm(0.5f, 0.5f, 0.5f);
            vTaskDelay(delay_ticks);
            continue;
        }

        if (!enabled_.load(std::memory_order_relaxed)) {
            apply_pwm(0.5f, 0.5f, 0.5f);
            vTaskDelay(delay_ticks);
            continue;
        }

        const float mech_angle = (static_cast<float>(raw_angle) / 16384.0f) * kTwoPi;
        const float detent_angle = mech_angle * static_cast<float>(config_.detent_positions);
        const float torque = -std::sin(detent_angle);
        const float gain = std::clamp(config_.detent_strength * strength_scale_.load(std::memory_order_relaxed), 0.0f, 1.0f);
        const float torque_cmd = std::clamp(gain * torque, -1.0f, 1.0f);

        float electrical_angle = mech_angle * static_cast<float>(config_.pole_pairs);
        electrical_angle *= static_cast<float>(config_.sensor_direction);
        electrical_angle += electrical_offset_;
        const float amplitude = max_ratio * torque_cmd;

        auto phase_value = [&](float phase_shift) {
            float value = 0.5f + amplitude * std::sin(electrical_angle + phase_shift);
            return std::clamp(value, 0.0f, 1.0f);
        };

        apply_pwm(phase_value(0.0f), phase_value(-kPhaseShift), phase_value(+kPhaseShift));
        vTaskDelay(delay_ticks);
    }
}

void MotorController::apply_pwm(float a, float b, float c) {
    const uint32_t max_duty = (1u << config_.resolution_bits) - 1u;
    const float phases[3] = {a, b, c};

    for (int i = 0; i < 3; ++i) {
        float clamped = std::clamp(phases[i], 0.0f, 1.0f);
        uint32_t duty = static_cast<uint32_t>(clamped * static_cast<float>(max_duty));
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, kChannels[i], duty);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, kChannels[i]);
    }
}

}  // namespace dial
