#pragma once

#include <Arduino.h>

namespace cfg {

constexpr uint32_t SERIAL_MONITOR_BAUD = 115200;
constexpr uint32_t VEBUS_BAUD = 256000;
constexpr uint32_t MODBUS_DEFAULT_BAUD = 19200;

// Board and Rev-A carrier GPIO assignments are defined in
// include/board/BoardPins.h. Both field buses use independent UART signal sets
// and explicit direction control.

} // namespace cfg
