#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "Rs485Settings.h"

namespace multibus {
namespace modbus {

class Rs485SettingsStore {
public:
    bool begin() {
        return prefs_.begin("mbserial", false);
    }

    void end() {
        prefs_.end();
    }

    bool load(Rs485SerialSettings& settings) const {
        const size_t length = prefs_.getBytesLength("serial");
        if (length == 0) {
            settings = Rs485SerialSettings{};
            return true;
        }
        if (length != kRecordSize) return false;

        uint8_t record[kRecordSize] = {0};
        if (prefs_.getBytes("serial", record, sizeof(record)) != sizeof(record)) return false;
        if (record[0] != kRecordVersion) return false;

        settings = Rs485SerialSettings{};
        settings.baudRate = static_cast<uint32_t>(record[1]) |
                            (static_cast<uint32_t>(record[2]) << 8U) |
                            (static_cast<uint32_t>(record[3]) << 16U) |
                            (static_cast<uint32_t>(record[4]) << 24U);
        settings.dataBits = record[5];
        settings.stopBits = static_cast<Rs485StopBits>(record[6]);
        settings.parity = static_cast<Rs485Parity>(record[7]);
        return validRs485SerialSettings(settings);
    }

    bool save(const Rs485SerialSettings& settings) {
        if (!validRs485SerialSettings(settings)) return false;
        uint8_t record[kRecordSize] = {0};
        record[0] = kRecordVersion;
        record[1] = static_cast<uint8_t>(settings.baudRate & 0xffU);
        record[2] = static_cast<uint8_t>((settings.baudRate >> 8U) & 0xffU);
        record[3] = static_cast<uint8_t>((settings.baudRate >> 16U) & 0xffU);
        record[4] = static_cast<uint8_t>((settings.baudRate >> 24U) & 0xffU);
        record[5] = settings.dataBits;
        record[6] = static_cast<uint8_t>(settings.stopBits);
        record[7] = static_cast<uint8_t>(settings.parity);
        return prefs_.putBytes("serial", record, sizeof(record)) == sizeof(record);
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
