#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "DeviceConfig.h"

namespace multibus {

class BackupService {
public:
    static constexpr uint32_t BACKUP_SCHEMA_VERSION = 1;

    bool exportConfig(const DeviceConfig& config, String& output) const {
        JsonDocument doc;
        doc["format"] = "multibus-backup";
        doc["backup_schema"] = BACKUP_SCHEMA_VERSION;
        doc["config_schema"] = config.schemaVersion;
        doc["device_family"] = "esp32-multibus-gateway";
        doc["board"] = "HTIT-WB32LAF-V4.2";
        doc["contains_secrets"] = true;
        doc["encrypted"] = false;

        JsonObject components = doc["components"].to<JsonObject>();
        components["victron"] = modeValue(config.components.victron);
        components["lora"] = modeValue(config.components.lora);
        components["modbus"] = modeValue(config.components.modbus);
        components["gnss"] = modeValue(config.components.gnss);

        JsonObject network = doc["network"].to<JsonObject>();
        network["ssid"] = config.network.ssid;
        network["password"] = config.network.password;
        network["hostname"] = config.network.hostname;
        network["friendly_name"] = config.network.friendlyName;

        output = "";
        serializeJsonPretty(doc, output);
        return !output.isEmpty();
    }

    bool importConfig(const String& input, DeviceConfig& output, String& error) const {
        JsonDocument doc;
        const DeserializationError parseError = deserializeJson(doc, input);
        if (parseError) {
            error = String("invalid-json: ") + parseError.c_str();
            return false;
        }

        if (String(doc["format"] | "") != "multibus-backup") {
            error = "unsupported-format";
            return false;
        }
        if ((doc["backup_schema"] | 0U) != BACKUP_SCHEMA_VERSION) {
            error = "unsupported-backup-schema";
            return false;
        }
        if ((doc["config_schema"] | 0U) != DEVICE_CONFIG_SCHEMA_VERSION) {
            error = "unsupported-config-schema";
            return false;
        }
        if (String(doc["device_family"] | "") != "esp32-multibus-gateway") {
            error = "wrong-device-family";
            return false;
        }
        if (String(doc["board"] | "") != "HTIT-WB32LAF-V4.2") {
            error = "unsupported-board";
            return false;
        }

        const JsonObjectConst components = doc["components"].as<JsonObjectConst>();
        const JsonObjectConst network = doc["network"].as<JsonObjectConst>();
        if (components.isNull() || network.isNull()) {
            error = "missing-configuration-section";
            return false;
        }

        DeviceConfig next;
        next.schemaVersion = DEVICE_CONFIG_SCHEMA_VERSION;

        if (!parseVictron(String(components["victron"] | "0"), next.components.victron) ||
            !parseLoRa(String(components["lora"] | "0"), next.components.lora) ||
            !parseModbus(String(components["modbus"] | "0"), next.components.modbus) ||
            !parseGnss(String(components["gnss"] | "0"), next.components.gnss)) {
            error = "invalid-component-mode";
            return false;
        }

        next.network.ssid = String(network["ssid"] | "");
        next.network.password = String(network["password"] | "");
        next.network.hostname = String(network["hostname"] | "");
        next.network.friendlyName = String(network["friendly_name"] | "");

        const auto validation = validateConfig(next.components);
        if (validation != ConfigValidationResult::Ok) {
            error = toString(validation);
            return false;
        }

        output = next;
        error = "";
        return true;
    }

private:
    static const char* modeValue(VictronMode mode) {
        return mode == VictronMode::Enabled ? "1" : "0";
    }

    static const char* modeValue(LoRaMode mode) {
        if (mode == LoRaMode::LoRaWAN) return "W";
        if (mode == LoRaMode::Meshtastic) return "M";
        return "0";
    }

    static const char* modeValue(ModbusMode mode) {
        if (mode == ModbusMode::Master) return "M";
        if (mode == ModbusMode::Slave) return "S";
        return "0";
    }

    static const char* modeValue(GnssMode mode) {
        return mode == GnssMode::Enabled ? "1" : "0";
    }

    static bool parseVictron(const String& value, VictronMode& mode) {
        if (value == "0") { mode = VictronMode::Disabled; return true; }
        if (value == "1") { mode = VictronMode::Enabled; return true; }
        return false;
    }

    static bool parseLoRa(const String& value, LoRaMode& mode) {
        if (value == "0") { mode = LoRaMode::Disabled; return true; }
        if (value == "W") { mode = LoRaMode::LoRaWAN; return true; }
        if (value == "M") { mode = LoRaMode::Meshtastic; return true; }
        return false;
    }

    static bool parseModbus(const String& value, ModbusMode& mode) {
        if (value == "0") { mode = ModbusMode::Disabled; return true; }
        if (value == "M") { mode = ModbusMode::Master; return true; }
        if (value == "S") { mode = ModbusMode::Slave; return true; }
        return false;
    }

    static bool parseGnss(const String& value, GnssMode& mode) {
        if (value == "0") { mode = GnssMode::Disabled; return true; }
        if (value == "1") { mode = GnssMode::Enabled; return true; }
        return false;
    }
};

} // namespace multibus
