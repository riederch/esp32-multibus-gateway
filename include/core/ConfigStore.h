#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "DeviceConfig.h"

namespace multibus {

class ConfigStore {
public:
    bool begin() {
        return prefs_.begin("multibus", false);
    }

    void end() {
        prefs_.end();
    }

    bool load(DeviceConfig& config) {
        if (!prefs_.isKey("schema")) {
            return false;
        }

        config.schemaVersion = prefs_.getUInt("schema", DEVICE_CONFIG_SCHEMA_VERSION);
        config.components.victron = static_cast<VictronMode>(prefs_.getUChar("victron", 0));
        config.components.lora = static_cast<LoRaMode>(prefs_.getUChar("lora", 0));
        config.components.modbus = static_cast<ModbusMode>(prefs_.getUChar("modbus", 0));
        config.components.gnss = static_cast<GnssMode>(prefs_.getUChar("gnss", 0));
        config.network.ssid = prefs_.getString("wifi_ssid", "");
        config.network.password = prefs_.getString("wifi_pass", "");
        config.network.hostname = prefs_.getString("hostname", "");
        config.network.friendlyName = prefs_.getString("friendly", "");

        return config.schemaVersion == DEVICE_CONFIG_SCHEMA_VERSION &&
               validateConfig(config.components) == ConfigValidationResult::Ok;
    }

    bool save(const DeviceConfig& config) {
        if (validateConfig(config.components) != ConfigValidationResult::Ok) {
            return false;
        }

        return prefs_.putUInt("schema", config.schemaVersion) > 0 &&
               prefs_.putUChar("victron", static_cast<uint8_t>(config.components.victron)) > 0 &&
               prefs_.putUChar("lora", static_cast<uint8_t>(config.components.lora)) > 0 &&
               prefs_.putUChar("modbus", static_cast<uint8_t>(config.components.modbus)) > 0 &&
               prefs_.putUChar("gnss", static_cast<uint8_t>(config.components.gnss)) > 0 &&
               prefs_.putString("wifi_ssid", config.network.ssid) > 0 &&
               prefs_.putString("wifi_pass", config.network.password) > 0 &&
               prefs_.putString("hostname", config.network.hostname) > 0 &&
               prefs_.putString("friendly", config.network.friendlyName) > 0;
    }

    void clear() {
        prefs_.clear();
    }

private:
    Preferences prefs_;
};

} // namespace multibus
