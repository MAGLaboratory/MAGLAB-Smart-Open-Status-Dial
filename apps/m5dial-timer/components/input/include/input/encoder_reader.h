#pragma once

#include <cstdint>
#include <optional>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

namespace dial {

struct EncoderSample {
    int32_t delta_ticks;
    uint32_t timestamp_us;
};

struct EncoderConfig {
    int32_t gpio_a;
    int32_t gpio_b;
    bool invert = false;
    uint32_t sample_queue_depth = 64;
};

class EncoderReader {
public:
    EncoderReader() = default;
    esp_err_t init(const EncoderConfig& config);
    QueueHandle_t queue() const { return sample_queue_; }

private:
    EncoderConfig config_{};
    QueueHandle_t sample_queue_ = nullptr;
};

extern EncoderReader g_encoder_reader;

}  // namespace dial
