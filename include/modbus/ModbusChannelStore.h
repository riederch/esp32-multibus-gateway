#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include "ModbusChannel.h"

namespace multibus {
namespace modbus {

class ModbusChannelStore {
public:
    bool begin() {
        return prefs_.begin("mbchannels", false);
    }

    void end() {
        prefs_.end();
    }

    bool load(std::vector<ChannelConfig>& channels) {
        channels.clear();
        for (uint8_t slot = 0; slot < kCompatibilitySlotCount; ++slot) {
            char key[5];
            makeKey(slot, key);
            const size_t length = prefs_.getBytesLength(key);
            if (length == 0) continue;
            if (length != kRecordSize) return false;

            uint8_t record[kRecordSize] = {0};
            if (prefs_.getBytes(key, record, sizeof(record)) != sizeof(record)) return false;

            ChannelConfig config;
            if (!decodeRecord(record, config) || config.slot != slot) return false;
            channels.push_back(config);
        }
        return true;
    }

    bool save(const ChannelConfig& config) {
        if (!validChannelConfig(config)) return false;
        uint8_t record[kRecordSize] = {0};
        encodeRecord(config, record);
        char key[5];
        makeKey(config.slot, key);
        return prefs_.putBytes(key, record, sizeof(record)) == sizeof(record);
    }

    bool remove(uint8_t slot) {
        if (slot >= kCompatibilitySlotCount) return false;
        char key[5];
        makeKey(slot, key);
        if (!prefs_.isKey(key)) return true;
        return prefs_.remove(key);
    }

    void clear() {
        prefs_.clear();
    }

private:
    static constexpr uint8_t kRecordVersion = 1;
    static constexpr size_t kRecordSize = 23;

    static void makeKey(uint8_t slot, char key[5]) {
        snprintf(key, 5, "c%02u", static_cast<unsigned>(slot));
    }

    static void encodeRecord(const ChannelConfig& config, uint8_t record[kRecordSize]) {
        record[0] = kRecordVersion;
        record[1] = config.slot;
        record[2] = config.slaveId;
        record[3] = static_cast<uint8_t>(config.address & 0xffU);
        record[4] = static_cast<uint8_t>((config.address >> 8U) & 0xffU);
        record[5] = static_cast<uint8_t>(config.dataType);
        record[6] = static_cast<uint8_t>((config.quantity & 0x0fU) |
                                         (config.signedValue ? 0x10U : 0x00U));
        const size_t nameLength = strnlen(config.name, kMaxChannelNameLength);
        record[7] = static_cast<uint8_t>(nameLength);
        if (nameLength > 0) memcpy(record + 8, config.name, nameLength);
    }

    static bool decodeRecord(const uint8_t record[kRecordSize], ChannelConfig& config) {
        if (record[0] != kRecordVersion || !validWireDataType(record[5])) return false;
        const size_t nameLength = record[7];
        if (nameLength > kMaxChannelNameLength) return false;

        config = ChannelConfig{};
        config.slot = record[1];
        config.slaveId = record[2];
        config.address = static_cast<uint16_t>(record[3]) |
                         (static_cast<uint16_t>(record[4]) << 8U);
        config.dataType = static_cast<WireDataType>(record[5]);
        config.quantity = record[6] & 0x0fU;
        config.signedValue = (record[6] & 0x10U) != 0;
        if (!setChannelName(config, reinterpret_cast<const char*>(record + 8), nameLength)) return false;
        return validChannelConfig(config);
    }

    Preferences prefs_;
};

} // namespace modbus
} // namespace multibus
