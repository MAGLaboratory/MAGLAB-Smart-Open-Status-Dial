#pragma once

namespace dial {

struct PinMap {
    // Rotary input (MT6701 angle sensor)
    static constexpr int ENCODER_SDA = 1;
    static constexpr int ENCODER_SCL = 2;
    static constexpr int ENCODER_I2C_ADDRESS = 0x40;
    static constexpr int ENCODER_BUTTON = 5;
    // Legacy quadrature placeholders (unused on production hardware)
    static constexpr int ENCODER_A = -1;
    static constexpr int ENCODER_B = -1;

    // Power management
    static constexpr int POWER_HOLD = 18;      // IO_ON_OFF
    static constexpr int POWER_OFF_PIN = 7;    // OFF_PIN
    static constexpr int POWER_OFF_RELEASE = 39;  // OFF_UP_PIN

    // GC9A01 display SPI pins (as in MaTouch Arduino firmware)
    static constexpr int LCD_MOSI = 12;
    static constexpr int LCD_SCLK = 11;
    static constexpr int LCD_CS = 10;
    static constexpr int LCD_DC = 14;
    static constexpr int LCD_RESET = 9;
    static constexpr int LCD_BACKLIGHT = 13;

    // Capacitive touch (FT3267) I2C lines
    static constexpr int TOUCH_SDA = 6;
    static constexpr int TOUCH_SCL = 8;
    static constexpr int TOUCH_INT = -1;  // interrupt not wired in reference design

    // Haptic motor driver (DRV8833 inputs)
    static constexpr int MOTOR_IN1 = 17;
    static constexpr int MOTOR_IN2 = 16;
    static constexpr int MOTOR_IN3 = 15;

    // Misc peripherals (placeholder for LED ring)
    static constexpr int RGB_LED_DATA = 21;
};

}  // namespace dial
