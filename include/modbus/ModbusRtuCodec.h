#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ModbusChannel.h"

namespace multibus {
namespace modbus {

enum class RtuDecodeStatus : uint8_t {
    Ok,
    InvalidConfig,
    BufferTooSmall,
    Truncated,
    CrcMismatch,
    SlaveMismatch,
    FunctionMismatch,
    ExceptionResponse,
    LengthMismatch,
};

enum class ScalarKind : uint8_t {
    Boolean,
    SignedInteger,
    UnsignedInteger,
    FloatingPoint,
};

struct DecodedScalar {
    ScalarKind kind = ScalarKind::UnsignedInteger;
    bool booleanValue = false;
    int64_t signedValue = 0;
    uint64_t unsignedValue = 0;
    double floatingValue = 0.0;
};

class ModbusRtuCodec {
public:
    static constexpr uint8_t kReadCoils = 0x01;
    static constexpr uint8_t kReadDiscreteInputs = 0x02;
    static constexpr uint8_t kReadHoldingRegisters = 0x03;
    static constexpr uint8_t kReadInputRegisters = 0x04;

    static uint16_t crc16(const uint8_t* data, size_t length) {
        uint16_t crc = 0xFFFFU;
        if (data == nullptr && length != 0) return crc;

        for (size_t i = 0; i < length; ++i) {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; ++bit) {
                const bool lsb = (crc & 0x0001U) != 0;
                crc >>= 1U;
                if (lsb) crc ^= 0xA001U;
            }
        }
        return crc;
    }

    static uint8_t functionCode(WireDataType type) {
        const uint8_t raw = static_cast<uint8_t>(type);
        if (raw == static_cast<uint8_t>(WireDataType::Coil)) return kReadCoils;
        if (raw == static_cast<uint8_t>(WireDataType::Discrete)) return kReadDiscreteInputs;
        if ((raw >= 0x02U && raw <= 0x0dU) || (raw >= 0x1aU && raw <= 0x21U)) {
            return kReadInputRegisters;
        }
        if ((raw >= 0x0eU && raw <= 0x19U) || (raw >= 0x22U && raw <= 0x29U)) {
            return kReadHoldingRegisters;
        }
        return 0;
    }

    static uint8_t registersPerValue(WireDataType type) {
        const uint8_t raw = static_cast<uint8_t>(type);
        if (raw <= 0x03U || (raw >= 0x0eU && raw <= 0x0fU)) return 1;
        if ((raw >= 0x04U && raw <= 0x0dU) || (raw >= 0x10U && raw <= 0x19U)) return 2;
        if (raw >= 0x1aU && raw <= 0x29U) return 4;
        return 0;
    }

    static uint16_t readQuantity(const ChannelConfig& config) {
        if (!validChannelConfig(config)) return 0;
        if (isBooleanType(config.dataType)) return config.quantity;

        const uint8_t wordsPerValue = registersPerValue(config.dataType);
        if (wordsPerValue == 0) return 0;
        return static_cast<uint16_t>(wordsPerValue) * config.quantity;
    }

    static RtuDecodeStatus buildReadRequest(const ChannelConfig& config,
                                            uint8_t* output,
                                            size_t capacity,
                                            size_t& written) {
        written = 0;
        if (!validChannelConfig(config)) return RtuDecodeStatus::InvalidConfig;
        if (output == nullptr || capacity < 8) return RtuDecodeStatus::BufferTooSmall;

        const uint8_t function = functionCode(config.dataType);
        const uint16_t quantity = readQuantity(config);
        if (function == 0 || quantity == 0) return RtuDecodeStatus::InvalidConfig;
        if (static_cast<uint32_t>(config.address) + quantity > 0x10000UL) {
            return RtuDecodeStatus::InvalidConfig;
        }

        output[0] = config.slaveId;
        output[1] = function;
        output[2] = static_cast<uint8_t>((config.address >> 8U) & 0xffU);
        output[3] = static_cast<uint8_t>(config.address & 0xffU);
        output[4] = static_cast<uint8_t>((quantity >> 8U) & 0xffU);
        output[5] = static_cast<uint8_t>(quantity & 0xffU);

        const uint16_t crc = crc16(output, 6);
        output[6] = static_cast<uint8_t>(crc & 0xffU);
        output[7] = static_cast<uint8_t>((crc >> 8U) & 0xffU);
        written = 8;
        return RtuDecodeStatus::Ok;
    }

    static RtuDecodeStatus decodeReadResponse(const ChannelConfig& config,
                                              const uint8_t* frame,
                                              size_t length,
                                              DecodedScalar values[2],
                                              uint8_t& valueCount,
                                              uint8_t& exceptionCode) {
        valueCount = 0;
        exceptionCode = 0;
        if (!validChannelConfig(config)) return RtuDecodeStatus::InvalidConfig;
        if (frame == nullptr || length < 5) return RtuDecodeStatus::Truncated;

        const uint16_t expectedCrc = crc16(frame, length - 2);
        const uint16_t receivedCrc = static_cast<uint16_t>(frame[length - 2]) |
                                     (static_cast<uint16_t>(frame[length - 1]) << 8U);
        if (expectedCrc != receivedCrc) return RtuDecodeStatus::CrcMismatch;
        if (frame[0] != config.slaveId) return RtuDecodeStatus::SlaveMismatch;

        const uint8_t expectedFunction = functionCode(config.dataType);
        if (frame[1] == static_cast<uint8_t>(expectedFunction | 0x80U)) {
            if (length != 5) return RtuDecodeStatus::LengthMismatch;
            exceptionCode = frame[2];
            return RtuDecodeStatus::ExceptionResponse;
        }
        if (frame[1] != expectedFunction) return RtuDecodeStatus::FunctionMismatch;

        const uint16_t quantity = readQuantity(config);
        const size_t expectedDataBytes = isBooleanType(config.dataType)
            ? static_cast<size_t>((quantity + 7U) / 8U)
            : static_cast<size_t>(quantity) * 2U;

        if (frame[2] != expectedDataBytes || length != expectedDataBytes + 5U) {
            return RtuDecodeStatus::LengthMismatch;
        }

        const uint8_t* data = frame + 3;
        for (uint8_t index = 0; index < config.quantity; ++index) {
            if (isBooleanType(config.dataType)) {
                values[index] = DecodedScalar{};
                values[index].kind = ScalarKind::Boolean;
                values[index].booleanValue = ((data[index / 8U] >> (index % 8U)) & 0x01U) != 0;
                continue;
            }

            const size_t bytesPerValue = static_cast<size_t>(registersPerValue(config.dataType)) * 2U;
            if (!decodeRegisterValue(config, data + index * bytesPerValue, bytesPerValue, values[index])) {
                return RtuDecodeStatus::InvalidConfig;
            }
        }

        valueCount = config.quantity;
        return RtuDecodeStatus::Ok;
    }

private:
    static bool decodeRegisterValue(const ChannelConfig& config,
                                    const uint8_t* raw,
                                    size_t length,
                                    DecodedScalar& value) {
        const WireDataType type = config.dataType;
        const uint8_t rawType = static_cast<uint8_t>(type);

        if (rawType == 0x08U || rawType == 0x09U || rawType == 0x14U || rawType == 0x15U) {
            if (length != 4) return false;
            const size_t offset = (rawType == 0x08U || rawType == 0x14U) ? 0U : 2U;
            const uint16_t selected = static_cast<uint16_t>(raw[offset] << 8U) | raw[offset + 1U];
            setInteger(config, selected, 16, value);
            return true;
        }

        const size_t width = orderedWidth(type);
        if (width == 0 || length != width) return false;

        uint8_t ordered[8] = {0};
        if (!orderBytes(type, raw, width, ordered)) return false;

        if (isFloatingPointType(type)) {
            value = DecodedScalar{};
            value.kind = ScalarKind::FloatingPoint;
            if (width == 4) {
                const uint32_t bits = static_cast<uint32_t>(ordered[0]) << 24U |
                                      static_cast<uint32_t>(ordered[1]) << 16U |
                                      static_cast<uint32_t>(ordered[2]) << 8U |
                                      static_cast<uint32_t>(ordered[3]);
                float scalar = 0.0f;
                memcpy(&scalar, &bits, sizeof(scalar));
                value.floatingValue = scalar;
                return true;
            }
            if (width == 8) {
                uint64_t bits = 0;
                for (size_t i = 0; i < 8; ++i) bits = (bits << 8U) | ordered[i];
                double scalar = 0.0;
                memcpy(&scalar, &bits, sizeof(scalar));
                value.floatingValue = scalar;
                return true;
            }
            return false;
        }

        uint64_t integer = 0;
        for (size_t i = 0; i < width; ++i) integer = (integer << 8U) | ordered[i];
        setInteger(config, integer, static_cast<uint8_t>(width * 8U), value);
        return true;
    }

    static void setInteger(const ChannelConfig& config,
                           uint64_t raw,
                           uint8_t widthBits,
                           DecodedScalar& value) {
        value = DecodedScalar{};
        if (!config.signedValue) {
            value.kind = ScalarKind::UnsignedInteger;
            value.unsignedValue = raw;
            return;
        }

        value.kind = ScalarKind::SignedInteger;
        if (widthBits >= 64) {
            value.signedValue = static_cast<int64_t>(raw);
            return;
        }

        const uint64_t signBit = uint64_t{1} << (widthBits - 1U);
        if ((raw & signBit) == 0) {
            value.signedValue = static_cast<int64_t>(raw);
            return;
        }

        const uint64_t mask = (~uint64_t{0}) << widthBits;
        value.signedValue = static_cast<int64_t>(raw | mask);
    }

    static size_t orderedWidth(WireDataType type) {
        const uint8_t raw = static_cast<uint8_t>(type);
        if ((raw >= 0x02U && raw <= 0x03U) || (raw >= 0x0eU && raw <= 0x0fU)) return 2;
        if ((raw >= 0x04U && raw <= 0x07U) || (raw >= 0x0aU && raw <= 0x0dU) ||
            (raw >= 0x10U && raw <= 0x13U) || (raw >= 0x16U && raw <= 0x19U)) return 4;
        if (raw >= 0x1aU && raw <= 0x29U) return 8;
        return 0;
    }

    static bool orderBytes(WireDataType type,
                           const uint8_t* raw,
                           size_t width,
                           uint8_t ordered[8]) {
        if (raw == nullptr || ordered == nullptr) return false;
        const uint8_t rawType = static_cast<uint8_t>(type);

        if (width == 2) {
            const bool swapped = rawType == 0x03U || rawType == 0x0fU;
            ordered[0] = raw[swapped ? 1 : 0];
            ordered[1] = raw[swapped ? 0 : 1];
            return true;
        }

        if (width == 4) {
            const uint8_t variant = rawType <= 0x0dU
                ? static_cast<uint8_t>((rawType - 0x04U) & 0x03U)
                : static_cast<uint8_t>((rawType - 0x10U) & 0x03U);
            static const uint8_t kOrders[4][4] = {
                {0, 1, 2, 3},
                {1, 0, 3, 2},
                {2, 3, 0, 1},
                {3, 2, 1, 0},
            };
            for (size_t i = 0; i < 4; ++i) ordered[i] = raw[kOrders[variant][i]];
            return true;
        }

        if (width == 8) {
            const uint8_t variant = rawType <= 0x21U
                ? static_cast<uint8_t>((rawType - 0x1aU) & 0x03U)
                : static_cast<uint8_t>((rawType - 0x22U) & 0x03U);
            static const uint8_t kOrders[4][8] = {
                {0, 1, 2, 3, 4, 5, 6, 7},
                {6, 7, 4, 5, 2, 3, 0, 1},
                {1, 0, 3, 2, 5, 4, 7, 6},
                {7, 6, 5, 4, 3, 2, 1, 0},
            };
            for (size_t i = 0; i < 8; ++i) ordered[i] = raw[kOrders[variant][i]];
            return true;
        }

        return false;
    }
};

} // namespace modbus
} // namespace multibus
