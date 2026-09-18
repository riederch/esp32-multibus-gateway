#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FPort85Codec.h"
#include "FPort85ModbusUplink.h"
#include "modbus/ModbusChannel.h"
#include "modbus/ModbusRtuCodec.h"

namespace multibus::lorawan {

enum class AlarmKind : uint8_t {
    Threshold = 0x01,
    ThresholdRelease = 0x02,
    Change = 0x03,
};

class FPort85Alarm {
public:
    static EncodeStatus encode(const modbus::ChannelConfig& channel,
                               uint8_t registerIndex,
                               const modbus::DecodedScalar& value,
                               AlarmKind kind,
                               double changeValue,
                               uint8_t* output,
                               size_t capacity,
                               size_t& written) {
        written = 0;
        if (!modbus::validChannelConfig(channel) ||
            channel.slot >= modbus::kCompatibilitySlotCount ||
            registerIndex >= channel.quantity ||
            output == nullptr ||
            modbus::isBooleanType(channel.dataType)) {
            return EncodeStatus::Invalid;
        }

        uint8_t scalar[8] = {0};
        size_t scalarLength = 0;
        const EncodeStatus scalarStatus =
            FPort85ModbusUplink::encodeScalarBytes(
                channel, value, scalar, sizeof(scalar), scalarLength);
        if (scalarStatus != EncodeStatus::Ok) return scalarStatus;

        const size_t baseLength = 4U + scalarLength;
        const size_t totalLength =
            kind == AlarmKind::Change ? baseLength + 11U : baseLength;
        if (capacity < totalLength) return EncodeStatus::BufferTooSmall;

        output[0] = FPort85Codec::kModbusChannel;
        output[1] = FPort85Codec::kModbusChannelDataType;
        output[2] =
            static_cast<uint8_t>((static_cast<uint8_t>(kind) << 6U) |
                                 (channel.slot & 0x3fU));
        output[3] =
            static_cast<uint8_t>((modbus::uplinkSigned(channel) ? 0x80U : 0x00U) |
                                 (registerIndex == 1 ? 0x20U : 0x00U) |
                                 (static_cast<uint8_t>(
                                      modbus::uplinkDataType(channel.dataType)) &
                                  0x1fU));
        memcpy(output + 4, scalar, scalarLength);
        written = baseLength;

        if (kind != AlarmKind::Change) return EncodeStatus::Ok;

        output[written++] = FPort85Codec::kModbusChannel;
        output[written++] = 0x74;
        output[written++] =
            static_cast<uint8_t>((registerIndex == 1 ? 0x40U : 0x00U) |
                                 (channel.slot & 0x3fU));

        uint64_t bits = 0;
        memcpy(&bits, &changeValue, sizeof(bits));
        FPort85ModbusUplink::writeLittleEndian(bits, output + written, 8);
        written += 8;
        return EncodeStatus::Ok;
    }
};

} // namespace multibus::lorawan
