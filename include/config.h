#pragma once

#include <Arduino.h>

namespace cfg {

// UART assignments are intentionally separated so VE.Bus and Modbus never
// share a transceiver. Adjust GPIOs to the actual prototype wiring.
constexpr int VEBUS_UART_NUM = 1;
constexpr int VEBUS_TX_PIN = 17;
constexpr int VEBUS_RX_PIN = 18;
constexpr int VEBUS_DE_PIN = 16;
constexpr uint32_t VEBUS_BAUD = 256000;

constexpr int MODBUS_UART_NUM = 2;
constexpr int MODBUS_TX_PIN = 5;
constexpr int MODBUS_RX_PIN = 6;
constexpr int MODBUS_DE_PIN = 7;
constexpr uint32_t MODBUS_BAUD = 19200;

} // namespace cfg
