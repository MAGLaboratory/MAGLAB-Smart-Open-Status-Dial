#include "board/dial_board.h"

#include <algorithm>
#include <new>

#include <esp_check.h>
#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/gpio.h>

#include "board/pinmap.h"

namespace dial {

namespace {
constexpr const char* TAG = "DialBoard";
constexpr const char* TAG_DISPLAY = "DialDisplay";
constexpr const char* TAG_BACKLIGHT = "DialBacklight";
constexpr const char* TAG_TOUCH = "DialTouch";

constexpr uint8_t kCmdDelayFlag = 0x80;
constexpr uint8_t kCmdEndMarker = 0xFF;
constexpr uint8_t kFt3267Address = 0x38;

constexpr uint8_t kGc9a01InitSequence[] = {
    0xEF, 0,
    0xEB, 1, 0x14,
    0xFE, 0,
    0xEF, 0,
    0xEB, 1, 0x14,
    0x84, 1, 0x40,
    0x85, 1, 0xFF,
    0x86, 1, 0xFF,
    0x87, 1, 0xFF,
    0x8E, 1, 0xFF,
    0x8F, 1, 0xFF,
    0x88, 1, 0x0A,
    0x89, 1, 0x21,
    0x8A, 1, 0x00,
    0x8B, 1, 0x80,
    0x8C, 1, 0x01,
    0x8D, 1, 0x01,
    0xB6, 2, 0x00, 0x20,
    0x90, 4, 0x08, 0x08, 0x08, 0x08,
    0xBD, 1, 0x06,
    0xBC, 1, 0x00,
    0xFF, 3, 0x60, 0x01, 0x04,
    0xC3, 1, 0x13,
    0xC4, 1, 0x13,
    0xC9, 1, 0x22,
    0xBE, 1, 0x11,
    0xE1, 2, 0x10, 0x0E,
    0xDF, 3, 0x21, 0x0C, 0x02,
    0xF0, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
    0xF1, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
    0xF2, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
    0xF3, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
    0xED, 2, 0x1B, 0x0B,
    0xAE, 1, 0x77,
    0xCD, 1, 0x63,
    0x70, 9, 0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03,
    0xE8, 1, 0x34,
    0x62,12, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70,
    0x63,12, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70,
    0x64, 7, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07,
    0x66,10, 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00,
    0x67,10, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98,
    0x74, 7, 0x10, 0x68, 0x80, 0x00, 0x00, 0x4E, 0x00,
    0x98, 2, 0x3E, 0x07,
    0x35, 1, 0x00,
    0x11, static_cast<uint8_t>(0 | kCmdDelayFlag), 120,
    0x29, 0,
    kCmdEndMarker, kCmdEndMarker,
};

constexpr size_t area_buffer_size_bytes(int width, int height) {
    return static_cast<size_t>(width) * static_cast<size_t>(height) * sizeof(uint16_t);
}

inline TickType_t ms_to_ticks(uint32_t ms) {
    return pdMS_TO_TICKS(ms);
}

}  // namespace

static esp_err_t ensure_gpio_output(int gpio_num, int initial_level) {
    auto gpio = static_cast<gpio_num_t>(gpio_num);
    ESP_RETURN_ON_ERROR(gpio_reset_pin(gpio), TAG, "gpio_reset_pin failed (%d)", gpio_num);
    ESP_RETURN_ON_ERROR(gpio_set_direction(gpio, GPIO_MODE_OUTPUT), TAG, "gpio_set_direction failed (%d)", gpio_num);
    ESP_RETURN_ON_ERROR(gpio_set_level(gpio, initial_level), TAG, "gpio_set_level failed (%d)", gpio_num);
    return ESP_OK;
}

// ------------------------------- Backlight ---------------------------------

esp_err_t Backlight::init() {
    if (initialized_) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(ensure_gpio_output(PinMap::LCD_BACKLIGHT, 0), TAG_BACKLIGHT, "backlight gpio init failed");

    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode = mode_;
    timer_cfg.timer_num = timer_;
    timer_cfg.duty_resolution = resolution_;
    timer_cfg.freq_hz = 20000;
    timer_cfg.clk_cfg = LEDC_AUTO_CLK;
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG_BACKLIGHT, "timer config failed");

    ledc_channel_config_t channel_cfg = {};
    channel_cfg.gpio_num = PinMap::LCD_BACKLIGHT;
    channel_cfg.speed_mode = mode_;
    channel_cfg.channel = channel_;
    channel_cfg.timer_sel = timer_;
    channel_cfg.duty = 0;
    channel_cfg.hpoint = 0;
    channel_cfg.flags.output_invert = 0;
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_cfg), TAG_BACKLIGHT, "channel config failed");

    initialized_ = true;
    return set_brightness(1.0f);
}

esp_err_t Backlight::set_brightness(float ratio) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    ratio = std::clamp(ratio, 0.0f, 1.0f);
    const uint32_t max_duty = (1u << static_cast<uint32_t>(resolution_)) - 1u;
    const uint32_t duty = static_cast<uint32_t>(ratio * static_cast<float>(max_duty) + 0.5f);

    ESP_RETURN_ON_ERROR(ledc_set_duty(mode_, channel_, duty), TAG_BACKLIGHT, "set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(mode_, channel_), TAG_BACKLIGHT, "update duty failed");
    return ESP_OK;
}

// ------------------------------- Display -----------------------------------

esp_err_t Display::perform_reset() {
    ESP_RETURN_ON_ERROR(ensure_gpio_output(PinMap::LCD_RESET, 1), TAG_DISPLAY, "reset gpio init failed");
    vTaskDelay(ms_to_ticks(10));
    ESP_RETURN_ON_ERROR(gpio_set_level(static_cast<gpio_num_t>(PinMap::LCD_RESET), 0), TAG_DISPLAY, "reset low failed");
    vTaskDelay(ms_to_ticks(20));
    ESP_RETURN_ON_ERROR(gpio_set_level(static_cast<gpio_num_t>(PinMap::LCD_RESET), 1), TAG_DISPLAY, "reset high failed");
    vTaskDelay(ms_to_ticks(20));
    return ESP_OK;
}

bool Display::on_color_trans_done(esp_lcd_panel_io_handle_t /*io*/, esp_lcd_panel_io_event_data_t* /*edata*/, void* user_ctx) {
    auto* self = static_cast<Display*>(user_ctx);
    if (!self || !self->flush_done_sem_) {
        return false;
    }
    BaseType_t higher_priority_woken = pdFALSE;
    xSemaphoreGiveFromISR(self->flush_done_sem_, &higher_priority_woken);
    return higher_priority_woken == pdTRUE;
}

esp_err_t Display::send_init_sequence() {
    const uint8_t* sequence = kGc9a01InitSequence;
    while (!(sequence[0] == kCmdEndMarker && sequence[1] == kCmdEndMarker)) {
        const uint8_t command = *sequence++;
        uint8_t length = *sequence++;
        uint32_t delay_ms = 0;
        if (length & kCmdDelayFlag) {
            length &= ~kCmdDelayFlag;
            delay_ms = *sequence++;
        }
        const uint8_t* params = sequence;
        sequence += length;

        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(panel_io_, command, params, length), TAG_DISPLAY, "cmd 0x%02X failed", command);

        if (delay_ms != 0) {
            vTaskDelay(ms_to_ticks(delay_ms));
        }
    }

    static constexpr uint8_t pixel_format = 0x55;  // RGB565
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(panel_io_, 0x3A, &pixel_format, sizeof(pixel_format)), TAG_DISPLAY, "pixel format set failed");

    static constexpr uint8_t madctl = 0x00;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(panel_io_, 0x36, &madctl, sizeof(madctl)), TAG_DISPLAY, "madctl set failed");

    return ESP_OK;
}

esp_err_t Display::init() {
    if (initialized_) {
        return ESP_OK;
    }

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = PinMap::LCD_MOSI;
    bus_cfg.miso_io_num = -1;
    bus_cfg.sclk_io_num = PinMap::LCD_SCLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = area_buffer_size_bytes(width_, height_);
    bus_cfg.flags = SPICOMMON_BUSFLAG_MASTER;

    esp_err_t bus_rc = spi_bus_initialize(spi_host_, &bus_cfg, SPI_DMA_CH_AUTO);
    if (bus_rc == ESP_ERR_INVALID_STATE) {
        bus_rc = ESP_OK;  // Bus already initialised elsewhere.
    }
    ESP_RETURN_ON_ERROR(bus_rc, TAG_DISPLAY, "spi bus init failed");

    if (flush_done_sem_ == nullptr) {
        flush_done_sem_ = xSemaphoreCreateBinary();
        if (!flush_done_sem_) {
            ESP_LOGE(TAG_DISPLAY, "Failed to allocate flush semaphore");
            return ESP_ERR_NO_MEM;
        }
    }

    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.cs_gpio_num = static_cast<gpio_num_t>(PinMap::LCD_CS);
    io_cfg.dc_gpio_num = static_cast<gpio_num_t>(PinMap::LCD_DC);
    io_cfg.spi_mode = 0;
    io_cfg.pclk_hz = 40 * 1000 * 1000;
    io_cfg.trans_queue_depth = 10;
    io_cfg.on_color_trans_done = &Display::on_color_trans_done;
    io_cfg.user_ctx = this;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 8;
    io_cfg.flags.sio_mode = 1;  // MOSI only

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(spi_host_, &io_cfg, &panel_io_), TAG_DISPLAY, "panel io create failed");

    ESP_RETURN_ON_ERROR(perform_reset(), TAG_DISPLAY, "panel reset failed");
    ESP_RETURN_ON_ERROR(send_init_sequence(), TAG_DISPLAY, "panel init sequence failed");

    initialized_ = true;
    ESP_LOGI(TAG_DISPLAY, "GC9A01 initialised (%dx%d)", width_, height_);
    return ESP_OK;
}

esp_err_t Display::flush(const DisplayRegion& region, const uint16_t* pixel_data) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!pixel_data) {
        return ESP_ERR_INVALID_ARG;
    }
    if (region.width <= 0 || region.height <= 0) {
        return ESP_OK;
    }

    const int32_t x1 = std::max<int32_t>(0, region.x);
    const int32_t y1 = std::max<int32_t>(0, region.y);
    const int32_t x2 = std::min<int32_t>(width_ - 1, region.x + region.width - 1);
    const int32_t y2 = std::min<int32_t>(height_ - 1, region.y + region.height - 1);
    if (x2 < x1 || y2 < y1) {
        return ESP_OK;
    }

    const uint8_t column_params[] = {
        static_cast<uint8_t>((x1 >> 8) & 0xFF), static_cast<uint8_t>(x1 & 0xFF),
        static_cast<uint8_t>((x2 >> 8) & 0xFF), static_cast<uint8_t>(x2 & 0xFF),
    };
    const uint8_t row_params[] = {
        static_cast<uint8_t>((y1 >> 8) & 0xFF), static_cast<uint8_t>(y1 & 0xFF),
        static_cast<uint8_t>((y2 >> 8) & 0xFF), static_cast<uint8_t>(y2 & 0xFF),
    };

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(panel_io_, 0x2A, column_params, sizeof(column_params)), TAG_DISPLAY, "set column failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(panel_io_, 0x2B, row_params, sizeof(row_params)), TAG_DISPLAY, "set row failed");

    const size_t pixel_count = static_cast<size_t>(x2 - x1 + 1) * static_cast<size_t>(y2 - y1 + 1);
    const size_t byte_count = pixel_count * sizeof(uint16_t);

    if (flush_done_sem_) {
        xSemaphoreTake(flush_done_sem_, 0);
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(panel_io_, 0x2C, pixel_data, byte_count), TAG_DISPLAY, "tx color failed");

    if (flush_done_sem_) {
        xSemaphoreTake(flush_done_sem_, portMAX_DELAY);
    }
    return ESP_OK;
}

// ------------------------------- Touch -------------------------------------

esp_err_t TouchController::write_reg(uint8_t reg, uint8_t value) {
    const uint8_t buf[2] = {reg, value};
    return i2c_master_write_to_device(port_, kFt3267Address, buf, sizeof(buf), ms_to_ticks(20));
}

esp_err_t TouchController::read_regs(uint8_t reg, uint8_t* data, size_t length) {
    if (length == 0) {
        return ESP_OK;
    }
    return i2c_master_write_read_device(port_, kFt3267Address, &reg, 1, data, length, ms_to_ticks(20));
}

esp_err_t TouchController::init() {
    if (initialized_) {
        return ESP_OK;
    }

    i2c_config_t cfg = {};
    cfg.mode = I2C_MODE_MASTER;
    cfg.sda_io_num = static_cast<gpio_num_t>(PinMap::TOUCH_SDA);
    cfg.scl_io_num = static_cast<gpio_num_t>(PinMap::TOUCH_SCL);
    cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.master.clk_speed = 400000;
    cfg.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;
    ESP_RETURN_ON_ERROR(i2c_param_config(port_, &cfg), TAG_TOUCH, "i2c param config failed");

    esp_err_t install_rc = i2c_driver_install(port_, cfg.mode, 0, 0, 0);
    if (install_rc == ESP_ERR_INVALID_STATE) {
        install_rc = ESP_OK;
    }
    ESP_RETURN_ON_ERROR(install_rc, TAG_TOUCH, "i2c driver install failed");

    int touch_int_pin = PinMap::TOUCH_INT;
    if (touch_int_pin >= 0) {
        gpio_config_t int_cfg = {};
        int_cfg.pin_bit_mask = 1ULL << static_cast<unsigned>(touch_int_pin);
        int_cfg.mode = GPIO_MODE_INPUT;
        int_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        int_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        int_cfg.intr_type = GPIO_INTR_DISABLE;
        ESP_RETURN_ON_ERROR(gpio_config(&int_cfg), TAG_TOUCH, "touch int gpio failed");
    }

    ESP_RETURN_ON_ERROR(write_reg(0x80, 70), TAG_TOUCH, "cfg THGROUP failed");
    ESP_RETURN_ON_ERROR(write_reg(0x81, 60), TAG_TOUCH, "cfg THPEAK failed");
    ESP_RETURN_ON_ERROR(write_reg(0x82, 16), TAG_TOUCH, "cfg THCAL failed");
    ESP_RETURN_ON_ERROR(write_reg(0x83, 60), TAG_TOUCH, "cfg THWATER failed");
    ESP_RETURN_ON_ERROR(write_reg(0x84, 10), TAG_TOUCH, "cfg THTEMP failed");
    ESP_RETURN_ON_ERROR(write_reg(0x85, 20), TAG_TOUCH, "cfg THDIFF failed");
    ESP_RETURN_ON_ERROR(write_reg(0x86, 0), TAG_TOUCH, "cfg CTRL failed");
    ESP_RETURN_ON_ERROR(write_reg(0x87, 12), TAG_TOUCH, "cfg PERIODACTIVE failed");
    ESP_RETURN_ON_ERROR(write_reg(0x88, 40), TAG_TOUCH, "cfg PERIODMONITOR failed");

    initialized_ = true;
    ESP_LOGI(TAG_TOUCH, "FT3267 initialised");
    return ESP_OK;
}

esp_err_t TouchController::read(TouchPoint* point) {
    if (!point) {
        return ESP_ERR_INVALID_ARG;
    }
    point->touched = false;
    point->x = 0;
    point->y = 0;

    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[5] = {0};
    esp_err_t rc = read_regs(0x02, data, sizeof(data));
    if (rc != ESP_OK) {
        return rc;
    }

    const uint8_t touches = data[0] & 0x0F;
    if (touches == 0) {
        return ESP_OK;
    }

    const uint16_t x = static_cast<uint16_t>(((data[1] & 0x0F) << 8) | data[2]);
    const uint16_t y = static_cast<uint16_t>(((data[3] & 0x0F) << 8) | data[4]);

    point->touched = true;
    point->x = x;
    point->y = y;
    return ESP_OK;
}

// ------------------------------- DialBoard ---------------------------------

DialBoard g_board;

esp_err_t DialBoard::init(const DialBoardConfig& config) {
    if (initialized_) {
        ESP_LOGW(TAG, "Dial board already initialised");
        return ESP_OK;
    }

    config_ = config;

    ESP_RETURN_ON_ERROR(ensure_gpio_output(PinMap::POWER_HOLD, 1), TAG, "power hold failed");

    if (config.enable_display) {
        backlight_ = std::make_unique<Backlight>();
        if (!backlight_) {
            return ESP_ERR_NO_MEM;
        }
        ESP_RETURN_ON_ERROR(backlight_->init(), TAG, "backlight init failed");

        display_ = std::make_unique<Display>();
        if (!display_) {
            return ESP_ERR_NO_MEM;
        }
        ESP_RETURN_ON_ERROR(display_->init(), TAG, "display init failed");
    }

    if (config.enable_touch) {
        touch_ = std::make_unique<TouchController>();
        if (!touch_) {
            return ESP_ERR_NO_MEM;
        }
        ESP_RETURN_ON_ERROR(touch_->init(), TAG, "touch init failed");
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Dial board initialised");
    return ESP_OK;
}

void DialBoard::update() {
    // Placeholder for background tasks (power management etc.).
}

}  // namespace dial
