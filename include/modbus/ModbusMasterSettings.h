#pragma once

#include <stdint.h>

namespace multibus {
namespace modbus {

enum class PassThroughMode : uint8_t {
    Disabled = 0x00,
    Active = 0x10,
    TwoWay = 0x11,
};

struct ModbusMasterSettings {
    uint16_t executionIntervalMs = 50;
    uint16_t maxResponseTimeMs = 500;
    uint8_t maxRetryTimes = 3;
    PassThroughMode passThroughMode = PassThroughMode::Disabled;
    uint8_t passThroughPort = 2;
};

inline bool validPassThroughPort(uint8_t port) {
    return (port >= 2 && port <= 84) || (port >= 86 && port <= 223);
}

inline bool validPassThroughMode(PassThroughMode mode) {
    return mode == PassThroughMode::Disabled ||
           mode == PassThroughMode::Active ||
           mode == PassThroughMode::TwoWay;
}

inline bool validModbusMasterSettings(const ModbusMasterSettings& settings) {
    return settings.executionIntervalMs >= 10 && settings.executionIntervalMs <= 1000 &&
           settings.maxResponseTimeMs >= 10 && settings.maxResponseTimeMs <= 60000 &&
           settings.maxRetryTimes <= 5 &&
           validPassThroughMode(settings.passThroughMode) &&
           validPassThroughPort(settings.passThroughPort);
}

inline bool runtimeSupportsModbusMasterSettings(const ModbusMasterSettings& settings) {
    if (!validModbusMasterSettings(settings)) return false;
    // Active and two-way pass-through require the raw RS485/LoRaWAN bridge,
    // which is deliberately not emulated by the polling master.
    return settings.passThroughMode == PassThroughMode::Disabled;
}

} // namespace modbus
} // namespace multibus
