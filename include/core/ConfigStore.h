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
        if (!prefs_.isKey("schema")) return false;

        const uint32_t storedSchema = prefs_.getUInt("schema", 0);
        if (storedSchema < 1 || storedSchema > DEVICE_CONFIG_SCHEMA_VERSION) return false;

        config = DeviceConfig{};
        config.schemaVersion = DEVICE_CONFIG_SCHEMA_VERSION;
        config.components.victron = static_cast<VictronMode>(prefs_.getUChar("victron", 0));
        config.components.lora = static_cast<LoRaMode>(prefs_.getUChar("lora", 0));
        config.components.modbus = static_cast<ModbusMode>(prefs_.getUChar("modbus", 0));
        config.components.gnss = static_cast<GnssMode>(prefs_.getUChar("gnss", 0));
        config.network.ssid = prefs_.getString("wifi_ssid", "");
        config.network.password = prefs_.getString("wifi_pass", "");
        config.network.hostname = prefs_.getString("hostname", "");
        config.network.friendlyName = prefs_.getString("friendly", "");

        if (storedSchema >= 2) {
            config.lorawan.joinEui = prefs_.getString("lw_join_eui", MULTIBUS_DEFAULT_JOIN_EUI);
            config.lorawan.appKey = prefs_.getString("lw_app_key", "");
            config.lorawan.classC = prefs_.getBool("lw_class_c", true);
            config.lorawan.extensionFPort = prefs_.getUChar("lw_ext_port", LORAWAN_DEFAULT_EXTENSION_FPORT);
        }

        needsSave_ = storedSchema != DEVICE_CONFIG_SCHEMA_VERSION;
        return validateConfig(config.components) == ConfigValidationResult::Ok;
    }

    bool save(const DeviceConfig& config) {
        if (config.schemaVersion != DEVICE_CONFIG_SCHEMA_VERSION ||
            validateConfig(config.components) != ConfigValidationResult::Ok) {
            return false;
        }

        prefs_.putUInt("schema", config.schemaVersion);
        prefs_.putUChar("victron", static_cast<uint8_t>(config.components.victron));
        prefs_.putUChar("lora", static_cast<uint8_t>(config.components.lora));
        prefs_.putUChar("modbus", static_cast<uint8_t>(config.components.modbus));
        prefs_.putUChar("gnss", static_cast<uint8_t>(config.components.gnss));
        prefs_.putString("wifi_ssid", config.network.ssid);
        prefs_.putString("wifi_pass", config.network.password);
        prefs_.putString("hostname", config.network.hostname);
        prefs_.putString("friendly", config.network.friendlyName);
        prefs_.putString("lw_join_eui", config.lorawan.joinEui);
        prefs_.putString("lw_app_key", config.lorawan.appKey);
        prefs_.putBool("lw_class_c", config.lorawan.classC);
        prefs_.putUChar("lw_ext_port", config.lorawan.extensionFPort);

        needsSave_ = false;
        return prefs_.getUInt("schema", 0) == DEVICE_CONFIG_SCHEMA_VERSION;
    }

    bool requiresSave() const { return needsSave_; }

    void clear() {
        prefs_.clear();
        needsSave_ = false;
    }

private:
    Preferences prefs_;
    bool needsSave_ = false;
};

} // namespace multibus
