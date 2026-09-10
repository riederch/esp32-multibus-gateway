#pragma once

#include "ComponentModes.h"

namespace multibus {

struct AppConfig {
    VictronMode victron = VictronMode::Disabled;
    LoRaMode lora = LoRaMode::Disabled;
    ModbusMode modbus = ModbusMode::Disabled;
    GnssMode gnss = GnssMode::Disabled;
};

enum class ConfigValidationResult : uint8_t {
    Ok = 0,
    MeshtasticNotImplemented,
};

inline ConfigValidationResult validateConfig(const AppConfig& config) {
    if (config.lora == LoRaMode::Meshtastic) {
        return ConfigValidationResult::MeshtasticNotImplemented;
    }
    return ConfigValidationResult::Ok;
}

inline const char* toString(ConfigValidationResult result) {
    switch (result) {
        case ConfigValidationResult::Ok:
            return "ok";
        case ConfigValidationResult::MeshtasticNotImplemented:
            return "meshtastic-not-implemented";
        default:
            return "unknown";
    }
}

} // namespace multibus
