#pragma once

#include <stddef.h>
#include <stdint.h>

namespace multibus {
namespace lorawan {

constexpr uint8_t kCompatibilityFPort = 85;

struct CommandHeader {
    uint8_t channelId = 0;
    uint8_t type = 0;
};

class FPort85Codec {
public:
    static constexpr uint8_t kSystemChannel = 0xFF;
    static constexpr uint8_t kModbusChannel = 0xF9;

    static constexpr uint8_t kReportIntervalType = 0x03;
    static constexpr uint8_t kRs485ConfigType = 0x78;
    static constexpr uint8_t kModbusGlobalConfigType = 0x79;
    static constexpr uint8_t kModbusChannelConfigType = 0xEF;
    static constexpr uint8_t kModbusChannelDataType = 0x73;

    static bool readHeader(const uint8_t* payload, size_t length, CommandHeader& header) {
        if (payload == nullptr || length < 2) return false;
        header.channelId = payload[0];
        header.type = payload[1];
        return true;
    }

    static bool isVerifiedCommandPrefix(const CommandHeader& header) {
        if (header.channelId == kSystemChannel && header.type == kReportIntervalType) return true;
        if (header.channelId == kModbusChannel && header.type == kRs485ConfigType) return true;
        if (header.channelId == kModbusChannel && header.type == kModbusGlobalConfigType) return true;
        if (header.channelId == kSystemChannel && header.type == kModbusChannelConfigType) return true;
        return false;
    }

    static bool isVerifiedUplinkPrefix(const CommandHeader& header) {
        return header.channelId == kModbusChannel && header.type == kModbusChannelDataType;
    }
};

} // namespace lorawan
} // namespace multibus
