#pragma once

#include <cstdint>

#include <esp_err.h>
#include <esp_lcd_panel_io.h>

#include <memory>

#include <driver/i2c.h>
#include <driver/ledc.h>
#include <driver/spi_master.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace dial {

struct DialBoardConfig {
    bool enable_display = true;
    bool enable_touch = true;
    bool enable_led_ring = true;
    bool enable_speaker = true;
    bool enable_rtc = true;
    bool enable_ble = false;
};

struct DisplayRegion {
    int32_t x = 0;
    int32_t y = 0;
    int32_t width = 0;
    int32_t height = 0;
};

struct TouchPoint {
    bool touched = false;
    uint16_t x = 0;
    uint16_t y = 0;
};

class Backlight {
public:
    Backlight() = default;

    esp_err_t init();
    esp_err_t set_brightness(float ratio);
    bool initialized() const { return initialized_; }

private:
    bool initialized_ = false;
    ledc_mode_t mode_ = LEDC_LOW_SPEED_MODE;
    ledc_timer_t timer_ = LEDC_TIMER_0;
    ledc_channel_t channel_ = LEDC_CHANNEL_0;
    ledc_timer_bit_t resolution_ = LEDC_TIMER_10_BIT;
};

class Display {
public:
    Display() = default;

    esp_err_t init();
    esp_err_t flush(const DisplayRegion& region, const uint16_t* pixel_data);
    int width() const { return width_; }
    int height() const { return height_; }
    bool initialized() const { return initialized_; }

private:
    esp_err_t perform_reset();
    esp_err_t send_init_sequence();
    static bool on_color_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t* edata, void* user_ctx);

    bool initialized_ = false;
    spi_host_device_t spi_host_ = SPI3_HOST;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    SemaphoreHandle_t flush_done_sem_ = nullptr;
    int width_ = 240;
    int height_ = 240;
};

class TouchController {
public:
    TouchController() = default;

    esp_err_t init();
    esp_err_t read(TouchPoint* point);
    bool initialized() const { return initialized_; }

private:
    esp_err_t write_reg(uint8_t reg, uint8_t value);
    esp_err_t read_regs(uint8_t reg, uint8_t* data, size_t length);

    bool initialized_ = false;
    i2c_port_t port_ = I2C_NUM_0;
};

class DialBoard {
public:
    DialBoard() = default;

    esp_err_t init(const DialBoardConfig& config = {});
    void update();

    const DialBoardConfig& config() const { return config_; }

    Display& display() { return *display_; }
    TouchController& touch() { return *touch_; }
    Backlight& backlight() { return *backlight_; }

private:
    DialBoardConfig config_{};
    bool initialized_ = false;

    std::unique_ptr<Display> display_;
    std::unique_ptr<TouchController> touch_;
    std::unique_ptr<Backlight> backlight_;
};

extern DialBoard g_board;

}  // namespace dial
