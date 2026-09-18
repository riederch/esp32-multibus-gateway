#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FPort85Codec.h"

namespace multibus::lorawan {

struct BasicInfo {
    uint8_t serialNumber[8] = {0};
    uint8_t protocolVersion = 0x01;
    uint8_t tslMajor = 0x01;
    uint8_t tslMinor = 0x00;
    uint8_t hardwareMajor = 0x04;
    uint8_t hardwareMinor = 0x20;
    uint8_t softwareMajor = 0x00;
    uint8_t softwareMinor = 0x00;
    uint8_t deviceType = 0x02;
};

class FPort85BasicInfo {
public:
    static constexpr size_t kBasicInfoLength = 31;
    static constexpr size_t kBasicInfoWithResetLength = 34;

    static EncodeStatus encode(const BasicInfo& info,
                               bool includeResetEvent,
                               uint8_t* output,
                               size_t capacity,
                               size_t& written) {
        written = 0;
        const size_t required = includeResetEvent
            ? kBasicInfoWithResetLength
            : kBasicInfoLength;
        if (output == nullptr) return EncodeStatus::Invalid;
        if (capacity < required) return EncodeStatus::BufferTooSmall;
        if (info.deviceType > 0x03) return EncodeStatus::Invalid;

        size_t offset = 0;
        append3(output, offset, 0xff, 0x0b, 0xff);

        append3(output, offset, 0xff, 0x01, info.protocolVersion);

        output[offset++] = 0xff;
        output[offset++] = 0xff;
        output[offset++] = info.tslMajor;
        output[offset++] = info.tslMinor;

        output[offset++] = 0xff;
        output[offset++] = 0x16;
        memcpy(output + offset, info.serialNumber, sizeof(info.serialNumber));
        offset += sizeof(info.serialNumber);

        output[offset++] = 0xff;
        output[offset++] = 0x09;
        output[offset++] = info.hardwareMajor;
        output[offset++] = info.hardwareMinor;

        output[offset++] = 0xff;
        output[offset++] = 0x0a;
        output[offset++] = info.softwareMajor;
        output[offset++] = info.softwareMinor;

        append3(output, offset, 0xff, 0x0f, info.deviceType);

        if (includeResetEvent) {
            append3(output, offset, 0xff, 0xfe, 0xff);
        }

        written = offset;
        return written == required ? EncodeStatus::Ok : EncodeStatus::Invalid;
    }

private:
    static void append3(uint8_t* output,
                        size_t& offset,
                        uint8_t channel,
                        uint8_t type,
                        uint8_t value) {
        output[offset++] = channel;
        output[offset++] = type;
        output[offset++] = value;
    }
};

} // namespace multibus::lorawan
