#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FPort85Codec.h"
#include "modbus/ModbusRtuCodec.h"

namespace multibus {
namespace lorawan {

class FPort85ModbusUplink {
public:
    static EncodeStatus encodePollSuccess(const modbus::ChannelConfig& channel,
                                          const modbus::DecodedScalar* values,
                                          uint8_t valueCount,
                                          uint8_t* output,
                                          size_t capacity,
                                          size_t& written) {
        written = 0;
        if (!modbus::validChannelConfig(channel) || values == nullptr ||
            valueCount != channel.quantity || valueCount == 0 || valueCount > 2 ||
            output == nullptr) {
            return EncodeStatus::Invalid;
        }

        for (uint8_t index = 0; index < valueCount; ++index) {
            uint8_t data[8] = {0};
            size_t dataLength = 0;
            const EncodeStatus scalarStatus = encodeScalarBytes(channel, values[index], data, sizeof(data), dataLength);
            if (scalarStatus != EncodeStatus::Ok) return scalarStatus;

            size_t frameLength = 0;
            const EncodeStatus frameStatus = FPort85Codec::encodePeriodicValue(
                channel,
                index,
                data,
                dataLength,
                output + written,
                capacity - written,
                frameLength
            );
            if (frameStatus != EncodeStatus::Ok) return frameStatus;
            written += frameLength;
        }

        return EncodeStatus::Ok;
    }

    static EncodeStatus encodePollFailure(const modbus::ChannelConfig& channel,
                                          uint8_t* output,
                                          size_t capacity,
                                          size_t& written) {
        if (!modbus::validChannelConfig(channel)) {
            written = 0;
            return EncodeStatus::Invalid;
        }
        return FPort85Codec::encodeCollectionException(channel.slot, output, capacity, written);
    }

public:
    static EncodeStatus encodeScalarBytes(const modbus::ChannelConfig& channel,
                                          const modbus::DecodedScalar& scalar,
                                          uint8_t* output,
                                          size_t capacity,
                                          size_t& written) {
        written = 0;
        if (output == nullptr || !modbus::validChannelConfig(channel)) return EncodeStatus::Invalid;

        const modbus::UplinkDataType uplinkType = modbus::uplinkDataType(channel.dataType);
        const size_t width = modbus::uplinkValueWidth(uplinkType);
        if (width == 0) return EncodeStatus::Invalid;
        if (capacity < width) return EncodeStatus::BufferTooSmall;

        if (modbus::isBooleanType(channel.dataType)) {
            if (scalar.kind != modbus::ScalarKind::Boolean || width != 1) return EncodeStatus::Invalid;
            output[0] = scalar.booleanValue ? 1U : 0U;
            written = 1;
            return EncodeStatus::Ok;
        }

        if (modbus::isFloatingPointType(channel.dataType)) {
            if (scalar.kind != modbus::ScalarKind::FloatingPoint) return EncodeStatus::Invalid;
            if (width == 4) {
                const float value = static_cast<float>(scalar.floatingValue);
                uint32_t bits = 0;
                memcpy(&bits, &value, sizeof(bits));
                writeLittleEndian(bits, output, width);
                written = width;
                return EncodeStatus::Ok;
            }
            if (width == 8) {
                uint64_t bits = 0;
                const double value = scalar.floatingValue;
                memcpy(&bits, &value, sizeof(bits));
                writeLittleEndian(bits, output, width);
                written = width;
                return EncodeStatus::Ok;
            }
            return EncodeStatus::Invalid;
        }

        uint64_t raw = 0;
        if (modbus::uplinkSigned(channel)) {
            if (scalar.kind != modbus::ScalarKind::SignedInteger) return EncodeStatus::Invalid;
            raw = static_cast<uint64_t>(scalar.signedValue);
        } else {
            if (scalar.kind != modbus::ScalarKind::UnsignedInteger) return EncodeStatus::Invalid;
            raw = scalar.unsignedValue;
        }

        writeLittleEndian(raw, output, width);
        written = width;
        return EncodeStatus::Ok;
    }

    static void writeLittleEndian(uint64_t value, uint8_t* output, size_t width) {
        for (size_t i = 0; i < width; ++i) {
            output[i] = static_cast<uint8_t>((value >> (8U * i)) & 0xffU);
        }
    }
};

} // namespace lorawan
} // namespace multibus
