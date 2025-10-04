#include "input/encoder_reader.h"

#include <algorithm>
#include <cmath>

#include <esp_check.h>
#include <esp_log.h>
#include <esp_timer.h>

namespace dial {

namespace {
constexpr const char* TAG = "EncoderReader";
constexpr uint16_t kAngleResolution = 16384;  // 14-bit full scale
constexpr uint16_t kHalfResolution = kAngleResolution / 2;
}  // namespace

EncoderReader g_encoder_reader;

esp_err_t EncoderReader::init(const EncoderConfig& config) {
    config_ = config;

    if (config_.sda_gpio < 0 || config_.scl_gpio < 0) {
        ESP_LOGE(TAG, "I2C pins for MT6701 are not configured");
        return ESP_ERR_INVALID_ARG;
    }
    if (config_.ticks_per_revolution == 0) {
        ESP_LOGE(TAG, "ticks_per_revolution must be non-zero");
        return ESP_ERR_INVALID_ARG;
    }

    if (sample_queue_ == nullptr) {
        sample_queue_ = xQueueCreate(config_.sample_queue_depth, sizeof(EncoderSample));
        if (sample_queue_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create encoder queue");
            return ESP_ERR_NO_MEM;
        }
    }

    i2c_config_t i2c_cfg = {};
    i2c_cfg.mode = I2C_MODE_MASTER;
    i2c_cfg.sda_io_num = static_cast<gpio_num_t>(config_.sda_gpio);
    i2c_cfg.scl_io_num = static_cast<gpio_num_t>(config_.scl_gpio);
    i2c_cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_cfg.master.clk_speed = config_.i2c_clock_hz;
    i2c_cfg.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;
    ESP_RETURN_ON_ERROR(i2c_param_config(config_.i2c_port, &i2c_cfg), TAG, "i2c_param_config failed");

    esp_err_t install_rc = i2c_driver_install(config_.i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (install_rc != ESP_OK && install_rc != ESP_ERR_INVALID_STATE) {
        ESP_RETURN_ON_ERROR(install_rc, TAG, "i2c_driver_install failed");
    }
    i2c_ready_ = true;

    has_last_angle_ = false;
    residual_ticks_ = 0.0f;
    last_angle_raw_ = 0;

    if (task_handle_ == nullptr) {
        BaseType_t res = xTaskCreatePinnedToCore(
            &EncoderReader::task_entry,
            "mt6701_reader",
            config_.task_stack_size,
            this,
            config_.task_priority,
            &task_handle_,
            config_.task_core_id);
        if (res != pdPASS) {
            task_handle_ = nullptr;
            ESP_LOGE(TAG, "Failed to create MT6701 reader task");
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "MT6701 reader initialised (addr=0x%02X, sda=%d, scl=%d)",
             config_.i2c_address, config_.sda_gpio, config_.scl_gpio);
    return ESP_OK;
}

void EncoderReader::task_entry(void* arg) {
    auto* self = static_cast<EncoderReader*>(arg);
    self->run();
}

void EncoderReader::run() {
    const TickType_t delay_ticks = pdMS_TO_TICKS(std::max<uint32_t>(1, config_.poll_interval_ms));
    const float ticks_per_unit = static_cast<float>(config_.ticks_per_revolution) /
                                 static_cast<float>(kAngleResolution);

    while (true) {
        uint16_t raw = 0;
        if (read_raw_angle(&raw) == ESP_OK) {
            if (has_last_angle_) {
                int32_t delta = static_cast<int32_t>(raw) - static_cast<int32_t>(last_angle_raw_);
                if (delta > kHalfResolution) {
                    delta -= kAngleResolution;
                } else if (delta < -static_cast<int32_t>(kHalfResolution)) {
                    delta += kAngleResolution;
                }

                residual_ticks_ += static_cast<float>(delta) * ticks_per_unit;
                int32_t delta_ticks = static_cast<int32_t>(residual_ticks_);
                if (delta_ticks != 0) {
                    residual_ticks_ -= static_cast<float>(delta_ticks);
                    EncoderSample sample{
                        .delta_ticks = delta_ticks,
                        .timestamp_us = static_cast<uint32_t>(esp_timer_get_time()),
                    };
                    xQueueSend(sample_queue_, &sample, 0);
                }
            } else {
                has_last_angle_ = true;
            }
            last_angle_raw_ = raw;
        } else {
            has_last_angle_ = false;
            residual_ticks_ = 0.0f;
        }

        vTaskDelay(delay_ticks);
    }
}

esp_err_t EncoderReader::read_raw_angle(uint16_t* out_raw) {
    if (!i2c_ready_ || out_raw == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    const TickType_t timeout = pdMS_TO_TICKS(std::max<uint32_t>(1, config_.i2c_timeout_ms));

    uint8_t reg = 0x03;
    uint8_t msb = 0;
    esp_err_t err = i2c_master_write_read_device(
        config_.i2c_port,
        config_.i2c_address,
        &reg,
        1,
        &msb,
        1,
        timeout);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read MT6701 MSB: %s", esp_err_to_name(err));
        return err;
    }

    reg = 0x04;
    uint8_t lsb = 0;
    err = i2c_master_write_read_device(
        config_.i2c_port,
        config_.i2c_address,
        &reg,
        1,
        &lsb,
        1,
        timeout);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read MT6701 LSB: %s", esp_err_to_name(err));
        return err;
    }

    const uint16_t raw = (static_cast<uint16_t>(msb) << 6) | (static_cast<uint16_t>(lsb) & 0x3F);
    *out_raw = raw;
    return ESP_OK;
}

}  // namespace dial
