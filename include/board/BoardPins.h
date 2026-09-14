#pragma once

namespace multibus::board {

// Heltec WiFi LoRa 32 V4.2 / HTIT-WB32LAF verified onboard mapping.

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

// Heltec V4.2 GC1109 front-end module. These GPIOs are onboard radio resources
// and must never be allocated to carrier peripherals.
constexpr int LORA_FEM_POWER = 7;
constexpr int LORA_FEM_ENABLE = 2;
constexpr int LORA_FEM_PA = 46;

constexpr int USB_DM = 19;
constexpr int USB_DP = 20;

// Rev-A carrier field-bus allocation. UART signals use the ESP32-S3 GPIO matrix.
// MODBUS_RX was moved from GPIO2 to GPIO33 after verifying the V4.2 GC1109 FEM:
// GPIO2 is the onboard FEM enable signal and is therefore unavailable to Rev A.
constexpr int MODBUS_RX = 33;
constexpr int MODBUS_TX = 4;
constexpr int MODBUS_DIR = 5;

constexpr int VEBUS_RX = 47;
constexpr int VEBUS_TX = 48;
constexpr int VEBUS_DIR = 6;

// Reserved for optional isolated VE.Bus Standby / Panel Detect interfaces.
// The corresponding field-side circuitry is DNI until the electrical behaviour
// has been measured on the target MultiPlus.
constexpr int VEBUS_STB_CTRL = 43;
constexpr int VEBUS_PD_CTRL = 44;

// GPIO3 and GPIO45 remain unused carrier-side because they are ESP32-S3
// strapping pins. GPIO46 is also a strapping pin, but on Heltec V4.2 it is
// already consumed onboard by the GC1109 PA-mode control and is not available
// to the carrier. GPIO26 remains free for board-revision margin.

} // namespace multibus::board
