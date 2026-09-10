#pragma once

#include <stdint.h>

namespace multibus {

enum class VictronMode : uint8_t {
    Disabled = 0,
    Enabled = 1,
};

enum class LoRaMode : uint8_t {
    Disabled = 0,
    LoRaWAN = 'W',
    Meshtastic = 'M',
};

enum class ModbusMode : uint8_t {
    Disabled = 0,
    Master = 'M',
    Slave = 'S',
};

enum class GnssMode : uint8_t {
    Disabled = 0,
    Enabled = 1,
};

inline const char* toString(VictronMode mode) {
    return mode == VictronMode::Enabled ? "enabled" : "disabled";
}

inline const char* toString(LoRaMode mode) {
    switch (mode) {
        case LoRaMode::LoRaWAN: return "lorawan";
        case LoRaMode::Meshtastic: return "meshtastic";
        default: return "disabled";
    }
}

inline const char* toString(ModbusMode mode) {
    switch (mode) {
        case ModbusMode::Master: return "master";
        case ModbusMode::Slave: return "slave";
        default: return "disabled";
    }
}

inline const char* toString(GnssMode mode) {
    return mode == GnssMode::Enabled ? "enabled" : "disabled";
}

} // namespace multibus
