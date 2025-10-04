#pragma once

#include <cstdint>
#include <optional>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "driver/i2c.h"

namespace dial {

struct EncoderSample {
    int32_t delta_ticks;
    uint32_t timestamp_us;
};

struct EncoderConfig {
    int sda_gpio;
    int scl_gpio;
    i2c_port_t i2c_port = I2C_NUM_1;
    uint8_t i2c_address = 0x06;          // 7-bit address
    uint32_t sample_queue_depth = 64;
    uint32_t poll_interval_ms = 5;
    uint32_t ticks_per_revolution = 96;
    uint32_t i2c_clock_hz = 400000;
    uint32_t i2c_timeout_ms = 20;
    uint32_t task_stack_size = 3072;
    UBaseType_t task_priority = 5;
    BaseType_t task_core_id = 0;
};

class EncoderReader {
public:
    EncoderReader() = default;
    esp_err_t init(const EncoderConfig& config);
    QueueHandle_t queue() const { return sample_queue_; }

private:
    static void task_entry(void* arg);
    void run();
    esp_err_t read_raw_angle(uint16_t* out_raw);

    EncoderConfig config_{};
    QueueHandle_t sample_queue_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    bool has_last_angle_ = false;
    uint16_t last_angle_raw_ = 0;
    float residual_ticks_ = 0.0f;
    bool i2c_ready_ = false;
};

extern EncoderReader g_encoder_reader;

}  // namespace dial
