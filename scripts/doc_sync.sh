#!/usr/bin/env python3
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PINMAP = ROOT / "apps/m5dial-timer/components/board/include/board/pinmap.h"
OUTPUT = ROOT / "docs/m5dial_pinmap.md"

LABELS = {
    "ENCODER_SDA": "Encoder I2C SDA (MT6701)",
    "ENCODER_SCL": "Encoder I2C SCL (MT6701)",
    "ENCODER_I2C_ADDRESS": "Encoder I2C address",
    "ENCODER_A": "Quadrature phase A (unused)",
    "ENCODER_B": "Quadrature phase B (unused)",
    "ENCODER_SS0": "Encoder select (SS0)",
    "ENCODER_BUTTON": "Encoder push button",
    "POWER_HOLD": "Power latch (IO_ON_OFF)",
    "POWER_OFF_PIN": "Power-off request (OFF_PIN)",
    "POWER_OFF_RELEASE": "Power latch release (OFF_UP)",
    "LCD_MOSI": "Display SPI MOSI",
    "LCD_SCLK": "Display SPI SCLK",
    "LCD_CS": "Display chip select",
    "LCD_DC": "Display D/C (RS)",
    "LCD_RESET": "Display reset",
    "LCD_BACKLIGHT": "Backlight gate",
    "TOUCH_SDA": "Touch I2C SDA",
    "TOUCH_SCL": "Touch I2C SCL",
    "TOUCH_INT": "Touch interrupt",
    "TOUCH_RESET": "Touch reset",
    "MOTOR_IN1": "Motor driver IN1",
    "MOTOR_IN2": "Motor driver IN2",
    "MOTOR_IN3": "Motor driver IN3",
    "RGB_LED_DATA": "LED ring data (placeholder)",
    "BATTERY_SENSE": "Battery sense (BAT_AD divider)",
    "BOOT_STRAP": "Boot strap (IO0)",
}

pattern = re.compile(r"static constexpr int (\w+) = (-?(?:0x)?[0-9A-Fa-f]+);")
entries = []
for line in PINMAP.read_text().splitlines():
    match = pattern.search(line)
    if match:
        name, value_str = match.groups()
        entries.append((name, value_str, int(value_str, 0)))

entries.sort(key=lambda item: item[0])

with OUTPUT.open("w", encoding="utf-8") as f:
    f.write("# M5 Dial Pin Map\n\n")
    f.write("| Function | GPIO |\n")
    f.write("|----------|------|\n")
    for name, value_str, gpio in entries:
        label = LABELS.get(name, name.title().replace("_", " "))
        if gpio < 0:
            gpio_str = "(unused)"
        elif value_str.lower().startswith("0x"):
            gpio_str = value_str.lower()
        else:
            gpio_str = str(gpio)
        f.write(f"| {label} | {gpio_str} |\n")
print(f"Updated {OUTPUT.relative_to(ROOT)} with {len(entries)} entries")
