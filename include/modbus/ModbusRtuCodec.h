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
    static constexpr uint8_t kWriteSingleCoil = 0x05;
    static constexpr uint8_t kWriteSingleRegister = 0x06;
    static constexpr uint8_t kWriteMultipleRegisters = 0x10;

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

    static bool writableType(WireDataType type) {
        if (type == WireDataType::Coil) return true;
        const uint8_t raw = static_cast<uint8_t>(type);
        if (raw == 0x0eU || raw == 0x0fU) return true;
        if (raw >= 0x10U && raw <= 0x13U) return true;
        if (raw >= 0x16U && raw <= 0x19U) return true;
        if (raw >= 0x22U && raw <= 0x29U) return true;
        return false;
    }

    static RtuDecodeStatus buildWriteRequest(const ChannelConfig& config,
                                             uint8_t valueIndex,
                                             const DecodedScalar& value,
                                             uint8_t* output,
                                             size_t capacity,
                                             size_t& written) {
        written = 0;
        if (!validChannelConfig(config) || valueIndex >= config.quantity || !writableType(config.dataType)) {
            return RtuDecodeStatus::InvalidConfig;
        }
        if (output == nullptr) return RtuDecodeStatus::BufferTooSmall;

        if (config.dataType == WireDataType::Coil) {
            if (value.kind != ScalarKind::Boolean || capacity < 8) return RtuDecodeStatus::InvalidConfig;
            const uint32_t address = static_cast<uint32_t>(config.address) + valueIndex;
            if (address > 0xffffU) return RtuDecodeStatus::InvalidConfig;

            output[0] = config.slaveId;
            output[1] = kWriteSingleCoil;
            output[2] = static_cast<uint8_t>((address >> 8U) & 0xffU);
            output[3] = static_cast<uint8_t>(address & 0xffU);
            output[4] = value.booleanValue ? 0xffU : 0x00U;
            output[5] = 0x00U;
            appendCrc(output, 6);
            written = 8;
            return RtuDecodeStatus::Ok;
        }

        const uint8_t wordsPerValue = registersPerValue(config.dataType);
        if (wordsPerValue == 0) return RtuDecodeStatus::InvalidConfig;
        const uint32_t address = static_cast<uint32_t>(config.address) +
                                 static_cast<uint32_t>(valueIndex) * wordsPerValue;
        if (address + wordsPerValue > 0x10000UL) return RtuDecodeStatus::InvalidConfig;

        uint8_t wire[8] = {0};
        size_t wireLength = 0;
        if (!encodeRegisterValue(config, value, wire, sizeof(wire), wireLength)) {
            return RtuDecodeStatus::InvalidConfig;
        }

        if (wordsPerValue == 1) {
            if (capacity < 8 || wireLength != 2) return RtuDecodeStatus::BufferTooSmall;
            output[0] = config.slaveId;
            output[1] = kWriteSingleRegister;
            output[2] = static_cast<uint8_t>((address >> 8U) & 0xffU);
            output[3] = static_cast<uint8_t>(address & 0xffU);
            output[4] = wire[0];
            output[5] = wire[1];
            appendCrc(output, 6);
            written = 8;
            return RtuDecodeStatus::Ok;
        }

        const size_t frameLength = 9U + wireLength;
        if (capacity < frameLength) return RtuDecodeStatus::BufferTooSmall;
        output[0] = config.slaveId;
        output[1] = kWriteMultipleRegisters;
        output[2] = static_cast<uint8_t>((address >> 8U) & 0xffU);
        output[3] = static_cast<uint8_t>(address & 0xffU);
        output[4] = 0x00U;
        output[5] = wordsPerValue;
        output[6] = static_cast<uint8_t>(wireLength);
        memcpy(output + 7, wire, wireLength);
        appendCrc(output, 7U + wireLength);
        written = frameLength;
        return RtuDecodeStatus::Ok;
    }

    static RtuDecodeStatus decodeWriteResponse(const ChannelConfig& config,
                                               uint8_t valueIndex,
                                               const uint8_t* request,
                                               size_t requestLength,
                                               const uint8_t* response,
                                               size_t responseLength,
                                               uint8_t& exceptionCode) {
        exceptionCode = 0;
        if (!validChannelConfig(config) || valueIndex >= config.quantity ||
            !writableType(config.dataType) || request == nullptr || response == nullptr) {
            return RtuDecodeStatus::InvalidConfig;
        }
        if (responseLength < 5) return RtuDecodeStatus::Truncated;

        const uint16_t expectedCrc = crc16(response, responseLength - 2);
        const uint16_t receivedCrc = static_cast<uint16_t>(response[responseLength - 2]) |
                                     (static_cast<uint16_t>(response[responseLength - 1]) << 8U);
        if (expectedCrc != receivedCrc) return RtuDecodeStatus::CrcMismatch;
        if (response[0] != config.slaveId) return RtuDecodeStatus::SlaveMismatch;

        const uint8_t function = requestLength >= 2 ? request[1] : 0;
        if (response[1] == static_cast<uint8_t>(function | 0x80U)) {
            if (responseLength != 5) return RtuDecodeStatus::LengthMismatch;
            exceptionCode = response[2];
            return RtuDecodeStatus::ExceptionResponse;
        }
        if (response[1] != function) return RtuDecodeStatus::FunctionMismatch;

        if (function == kWriteSingleCoil || function == kWriteSingleRegister) {
            if (requestLength != 8 || responseLength != 8) return RtuDecodeStatus::LengthMismatch;
            return memcmp(request, response, 6) == 0
                ? RtuDecodeStatus::Ok
                : RtuDecodeStatus::LengthMismatch;
        }

        if (function == kWriteMultipleRegisters) {
            if (requestLength < 11 || responseLength != 8) return RtuDecodeStatus::LengthMismatch;
            if (memcmp(request, response, 6) != 0) return RtuDecodeStatus::LengthMismatch;
            return RtuDecodeStatus::Ok;
        }

        return RtuDecodeStatus::FunctionMismatch;
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
    static void appendCrc(uint8_t* frame, size_t payloadLength) {
        const uint16_t crc = crc16(frame, payloadLength);
        frame[payloadLength] = static_cast<uint8_t>(crc & 0xffU);
        frame[payloadLength + 1U] = static_cast<uint8_t>((crc >> 8U) & 0xffU);
    }

    static bool encodeRegisterValue(const ChannelConfig& config,
                                    const DecodedScalar& value,
                                    uint8_t* wire,
                                    size_t capacity,
                                    size_t& written) {
        written = 0;
        const size_t width = orderedWidth(config.dataType);
        if (width == 0 || wire == nullptr || capacity < width) return false;

        uint8_t canonical[8] = {0};
        if (isFloatingPointType(config.dataType)) {
            if (value.kind != ScalarKind::FloatingPoint) return false;
            if (width == 4) {
                const float scalar = static_cast<float>(value.floatingValue);
                uint32_t bits = 0;
                memcpy(&bits, &scalar, sizeof(bits));
                for (size_t i = 0; i < 4; ++i) {
                    canonical[i] = static_cast<uint8_t>((bits >> (8U * (3U - i))) & 0xffU);
                }
            } else if (width == 8) {
                uint64_t bits = 0;
                const double scalar = value.floatingValue;
                memcpy(&bits, &scalar, sizeof(bits));
                for (size_t i = 0; i < 8; ++i) {
                    canonical[i] = static_cast<uint8_t>((bits >> (8U * (7U - i))) & 0xffU);
                }
            } else {
                return false;
            }
        } else {
            uint64_t raw = 0;
            if (config.signedValue) {
                if (value.kind != ScalarKind::SignedInteger) return false;
                raw = static_cast<uint64_t>(value.signedValue);
            } else {
                if (value.kind != ScalarKind::UnsignedInteger) return false;
                raw = value.unsignedValue;
            }

            const uint8_t bits = static_cast<uint8_t>(width * 8U);
            if (bits < 64) {
                const uint64_t mask = (uint64_t{1} << bits) - 1U;
                if (config.signedValue) {
                    const int64_t minValue = -(int64_t{1} << (bits - 1U));
                    const int64_t maxValue = (int64_t{1} << (bits - 1U)) - 1;
                    if (value.signedValue < minValue || value.signedValue > maxValue) return false;
                } else if ((raw & ~mask) != 0) {
                    return false;
                }
                raw &= mask;
            }

            for (size_t i = 0; i < width; ++i) {
                canonical[i] = static_cast<uint8_t>((raw >> (8U * (width - 1U - i))) & 0xffU);
            }
        }

        if (!orderBytes(config.dataType, canonical, width, wire)) return false;
        written = width;
        return true;
    }

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
            uint8_t base = 0;
            if (rawType >= 0x04U && rawType <= 0x07U) base = 0x04U;
            else if (rawType >= 0x0aU && rawType <= 0x0dU) base = 0x0aU;
            else if (rawType >= 0x10U && rawType <= 0x13U) base = 0x10U;
            else if (rawType >= 0x16U && rawType <= 0x19U) base = 0x16U;
            else return false;

            const uint8_t variant = static_cast<uint8_t>(rawType - base);
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
