#pragma once

#include <Arduino.h>

namespace cfg {

constexpr uint32_t SERIAL_MONITOR_BAUD = 115200;

// Hardware pin assignments are intentionally NOT defined here.
// The previous values were placeholders from the early ESP32-S3 DevKit scaffold.
// The reference platform is now HTIT-WB32LAF V4.2 and its exact pin allocation
// must be validated against the board schematic before any field-bus hardware is
// enabled. Pin mappings will live in a dedicated board abstraction.

constexpr uint32_t VEBUS_BAUD = 256000;
constexpr uint32_t MODBUS_DEFAULT_BAUD = 19200;

} // namespace cfg
