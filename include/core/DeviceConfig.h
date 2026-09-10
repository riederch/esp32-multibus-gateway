#pragma once

#include <Arduino.h>
#include "AppConfig.h"

namespace multibus {

constexpr uint32_t DEVICE_CONFIG_SCHEMA_VERSION = 1;

struct NetworkConfig {
    String ssid;
    String password;
    String hostname;
    String friendlyName;

    bool configured() const {
        return !ssid.isEmpty();
    }
};

struct DeviceConfig {
    uint32_t schemaVersion = DEVICE_CONFIG_SCHEMA_VERSION;
    AppConfig components;
    NetworkConfig network;
};

} // namespace multibus
