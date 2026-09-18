#pragma once

#include <Arduino.h>
#include "AppConfig.h"

namespace multibus {

constexpr uint32_t DEVICE_CONFIG_SCHEMA_VERSION = 3;
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

struct MqttConfig {
    bool enabled = false;
    String host;
    uint16_t port = 1883;
    String username;
    String password;
    String topicPrefix = "multibus";
    uint16_t publishIntervalSeconds = 30;
    bool retainState = true;

    bool valid() const {
        if (port == 0 || publishIntervalSeconds < 1 || publishIntervalSeconds > 3600) return false;
        if (topicPrefix.isEmpty() || topicPrefix.length() > 64) return false;
        if (enabled && host.isEmpty()) return false;
        return true;
    }
};

struct DeviceConfig {
    uint32_t schemaVersion = DEVICE_CONFIG_SCHEMA_VERSION;
    AppConfig components;
    NetworkConfig network;
    LoRaWanConfig lorawan;
    MqttConfig mqtt;
};

} // namespace multibus
