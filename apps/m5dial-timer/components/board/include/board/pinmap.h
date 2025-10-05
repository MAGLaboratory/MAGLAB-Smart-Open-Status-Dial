#pragma once

namespace dial {

struct PinMap {
    // Rotary input (MT6701 angle sensor)
    static constexpr int ENCODER_SDA = 1;
    static constexpr int ENCODER_SCL = 2;
    static constexpr int ENCODER_SS0 = 42;
    static constexpr int ENCODER_I2C_ADDRESS = 0x06;
    static constexpr int ENCODER_BUTTON = 5;

    // Power management
    static constexpr int POWER_HOLD = 18;      // IO_ON_OFF

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
    static constexpr int TOUCH_INT = 40;
    static constexpr int TOUCH_RESET = 41;

    // Haptic motor driver (DRV8833 inputs)
    static constexpr int MOTOR_IN1 = 17;
    static constexpr int MOTOR_IN2 = 16;
    static constexpr int MOTOR_IN3 = 15;

    // Misc monitoring
    static constexpr int BATTERY_SENSE = 4;    // BAT_AD (voltage divider)
    static constexpr int BOOT_STRAP = 0;       // Boot pin pulled up, momentary SW to GND

};

}  // namespace dial
