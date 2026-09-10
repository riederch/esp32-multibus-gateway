#pragma once

namespace multibus::board {

// Heltec WiFi LoRa 32 V4.2 / HTIT-WB32LAF verified onboard mapping.
// Field-bus pins are intentionally not assigned here yet.

constexpr int USER_BUTTON = 0;
constexpr int STATUS_LED = 35;

constexpr int VEXT_CTRL = 36;
constexpr int BATTERY_ADC_CTRL = 37;
constexpr int BATTERY_ADC = 1;

constexpr int OLED_SDA = 17;
constexpr int OLED_SCL = 18;
constexpr int OLED_RST = 21;

constexpr int GNSS_POWER_CTRL = 34;
constexpr int GNSS_RX = 38;
constexpr int GNSS_TX = 39;
constexpr int GNSS_WAKE = 40;
constexpr int GNSS_PPS = 41;
constexpr int GNSS_RST = 42;

constexpr int LORA_NSS = 8;
constexpr int LORA_SCK = 9;
constexpr int LORA_MOSI = 10;
constexpr int LORA_MISO = 11;
constexpr int LORA_RST = 12;
constexpr int LORA_BUSY = 13;
constexpr int LORA_DIO1 = 14;

constexpr int USB_DM = 19;
constexpr int USB_DP = 20;

} // namespace multibus::board
