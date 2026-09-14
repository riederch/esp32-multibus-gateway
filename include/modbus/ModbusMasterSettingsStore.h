#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "ModbusMasterSettings.h"

namespace multibus {
namespace modbus {

class ModbusMasterSettingsStore {
public:
    bool begin() {
        return prefs_.begin("mbmaster", false);
    }

    void end() {
        prefs_.end();
    }

    bool load(ModbusMasterSettings& settings) const {
        const size_t length = prefs_.getBytesLength("settings");
        if (length == 0) {
            settings = ModbusMasterSettings{};
            return true;
        }
        if (length != kRecordSize) return false;

        uint8_t record[kRecordSize] = {0};
        if (prefs_.getBytes("settings", record, sizeof(record)) != sizeof(record)) return false;
        if (record[0] != kRecordVersion) return false;

        settings = ModbusMasterSettings{};
        settings.executionIntervalMs = static_cast<uint16_t>(record[1]) |
                                       (static_cast<uint16_t>(record[2]) << 8U);
        settings.maxResponseTimeMs = static_cast<uint16_t>(record[3]) |
                                     (static_cast<uint16_t>(record[4]) << 8U);
        settings.maxRetryTimes = record[5];
        settings.passThroughMode = static_cast<PassThroughMode>(record[6]);
        settings.passThroughPort = record[7];
        return validModbusMasterSettings(settings);
    }

    bool save(const ModbusMasterSettings& settings) {
        if (!validModbusMasterSettings(settings)) return false;

        uint8_t record[kRecordSize] = {0};
        record[0] = kRecordVersion;
        record[1] = static_cast<uint8_t>(settings.executionIntervalMs & 0xffU);
        record[2] = static_cast<uint8_t>((settings.executionIntervalMs >> 8U) & 0xffU);
        record[3] = static_cast<uint8_t>(settings.maxResponseTimeMs & 0xffU);
        record[4] = static_cast<uint8_t>((settings.maxResponseTimeMs >> 8U) & 0xffU);
        record[5] = settings.maxRetryTimes;
        record[6] = static_cast<uint8_t>(settings.passThroughMode);
        record[7] = settings.passThroughPort;
        return prefs_.putBytes("settings", record, sizeof(record)) == sizeof(record);
    }

    void clear() {
        prefs_.clear();
    }

private:
    static constexpr uint8_t kRecordVersion = 1;
    static constexpr size_t kRecordSize = 8;

    mutable Preferences prefs_;
};

} // namespace modbus
} // namespace multibus
