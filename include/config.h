#pragma once

#include <Arduino.h>

namespace cfg {

constexpr uint32_t SERIAL_MONITOR_BAUD = 115200;
constexpr uint32_t VEBUS_BAUD = 256000;
constexpr uint32_t MODBUS_DEFAULT_BAUD = 19200;

// Board-specific GPIO assignments belong to include/board/BoardPins.h.
// Field-bus GPIOs are enabled only after the final UART/DE/RE allocation has
// been validated against all HTIT-WB32LAF V4.2 onboard resources.

} // namespace cfg
