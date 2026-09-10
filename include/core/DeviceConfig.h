#pragma once

#include <Arduino.h>
#include "AppConfig.h"

namespace multibus {

constexpr uint32_t DEVICE_CONFIG_SCHEMA_VERSION = 2;
constexpr uint8_t LORAWAN_COMPATIBILITY_FPORT = 85;
constexpr uint8_t LORAWAN_DEFAULT_EXTENSION_FPORT = 86;
constexpr const char* MULTIBUS_DEFAULT_JOIN_EUI = "024D554C54494255";

struct NetworkConfig {
    String ssid;
    String password;
    String hostname;
    String friendlyName;

    bool configured() const {
        return !ssid.isEmpty();
    }
};

struct LoRaWanConfig {
    String joinEui = MULTIBUS_DEFAULT_JOIN_EUI;
    String appKey;
    bool classC = true;
    uint8_t extensionFPort = LORAWAN_DEFAULT_EXTENSION_FPORT;
};

struct DeviceConfig {
    uint32_t schemaVersion = DEVICE_CONFIG_SCHEMA_VERSION;
    AppConfig components;
    NetworkConfig network;
    LoRaWanConfig lorawan;
};

} // namespace multibus
