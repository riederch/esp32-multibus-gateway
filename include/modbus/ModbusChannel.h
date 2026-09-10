#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace multibus {
namespace modbus {

constexpr uint8_t kCompatibilitySlotCount = 32;
constexpr size_t kMaxChannelNameLength = 16;

enum class WireDataType : uint8_t {
    Coil = 0x00,
    Discrete = 0x01,
    Input16AB = 0x02,
    Input16BA = 0x03,
    Input32ABCD = 0x04,
    Input32BADC = 0x05,
    Input32CDAB = 0x06,
    Input32DCBA = 0x07,
    Input32Upper16 = 0x08,
    Input32Lower16 = 0x09,
    InputFloatABCD = 0x0a,
    InputFloatBADC = 0x0b,
    InputFloatCDAB = 0x0c,
    InputFloatDCBA = 0x0d,
    Hold16AB = 0x0e,
    Hold16BA = 0x0f,
    Hold32ABCD = 0x10,
    Hold32BADC = 0x11,
    Hold32CDAB = 0x12,
    Hold32DCBA = 0x13,
    Hold32Upper16 = 0x14,
    Hold32Lower16 = 0x15,
    HoldFloatABCD = 0x16,
    HoldFloatBADC = 0x17,
    HoldFloatCDAB = 0x18,
    HoldFloatDCBA = 0x19,
    InputDoubleABCDEFGH = 0x1a,
    InputDoubleGHEFCDAB = 0x1b,
    InputDoubleBADCFEHG = 0x1c,
    InputDoubleHGFEDCBA = 0x1d,
    Input64ABCDEFGH = 0x1e,
    Input64GHEFCDAB = 0x1f,
    Input64BADCFEHG = 0x20,
    Input64HGFEDCBA = 0x21,
    HoldDoubleABCDEFGH = 0x22,
    HoldDoubleGHEFCDAB = 0x23,
    HoldDoubleBADCFEHG = 0x24,
    HoldDoubleHGFEDCBA = 0x25,
    Hold64ABCDEFGH = 0x26,
    Hold64GHEFCDAB = 0x27,
    Hold64BADCFEHG = 0x28,
    Hold64HGFEDCBA = 0x29,
};

enum class UplinkDataType : uint8_t {
    Coil = 0x00,
    Discrete = 0x01,
    Input16 = 0x02,
    Hold16 = 0x03,
    Hold32 = 0x04,
    HoldFloat = 0x05,
    Input32 = 0x06,
    InputFloat = 0x07,
    Input32Upper16 = 0x08,
    Input32Lower16 = 0x09,
    Hold32Upper16 = 0x0a,
    Hold32Lower16 = 0x0b,
    Hold64 = 0x0c,
    HoldDouble = 0x0d,
    Input64 = 0x0e,
    InputDouble = 0x0f,
};

struct ChannelConfig {
    uint8_t slot = 0;
    uint8_t slaveId = 1;
    uint16_t address = 0;
    WireDataType dataType = WireDataType::Coil;
    uint8_t quantity = 1;
    bool signedValue = false;
    char name[kMaxChannelNameLength + 1] = {0};
};

inline bool validWireDataType(uint8_t value) {
    return value <= static_cast<uint8_t>(WireDataType::Hold64HGFEDCBA);
}

inline bool validChannelConfig(const ChannelConfig& config) {
    return config.slot < kCompatibilitySlotCount &&
           config.slaveId >= 1 && config.slaveId <= 247 &&
           config.quantity >= 1 && config.quantity <= 2 &&
           validWireDataType(static_cast<uint8_t>(config.dataType));
}

inline bool isBooleanType(WireDataType type) {
    return type == WireDataType::Coil || type == WireDataType::Discrete;
}

inline bool isFloatingPointType(WireDataType type) {
    const uint8_t raw = static_cast<uint8_t>(type);
    return (raw >= 0x0a && raw <= 0x0d) ||
           (raw >= 0x16 && raw <= 0x19) ||
           (raw >= 0x1a && raw <= 0x1d) ||
           (raw >= 0x22 && raw <= 0x25);
}

inline UplinkDataType uplinkDataType(WireDataType type) {
    const uint8_t raw = static_cast<uint8_t>(type);
    if (raw == 0x00) return UplinkDataType::Coil;
    if (raw == 0x01) return UplinkDataType::Discrete;
    if (raw >= 0x02 && raw <= 0x03) return UplinkDataType::Input16;
    if (raw >= 0x04 && raw <= 0x07) return UplinkDataType::Input32;
    if (raw == 0x08) return UplinkDataType::Input32Upper16;
    if (raw == 0x09) return UplinkDataType::Input32Lower16;
    if (raw >= 0x0a && raw <= 0x0d) return UplinkDataType::InputFloat;
    if (raw >= 0x0e && raw <= 0x0f) return UplinkDataType::Hold16;
    if (raw >= 0x10 && raw <= 0x13) return UplinkDataType::Hold32;
    if (raw == 0x14) return UplinkDataType::Hold32Upper16;
    if (raw == 0x15) return UplinkDataType::Hold32Lower16;
    if (raw >= 0x16 && raw <= 0x19) return UplinkDataType::HoldFloat;
    if (raw >= 0x1a && raw <= 0x1d) return UplinkDataType::InputDouble;
    if (raw >= 0x1e && raw <= 0x21) return UplinkDataType::Input64;
    if (raw >= 0x22 && raw <= 0x25) return UplinkDataType::HoldDouble;
    return UplinkDataType::Hold64;
}

inline size_t uplinkValueWidth(UplinkDataType type) {
    switch (type) {
        case UplinkDataType::Coil:
        case UplinkDataType::Discrete:
            return 1;
        case UplinkDataType::Input16:
        case UplinkDataType::Hold16:
            return 2;
        case UplinkDataType::Hold32:
        case UplinkDataType::HoldFloat:
        case UplinkDataType::Input32:
        case UplinkDataType::InputFloat:
        case UplinkDataType::Input32Upper16:
        case UplinkDataType::Input32Lower16:
        case UplinkDataType::Hold32Upper16:
        case UplinkDataType::Hold32Lower16:
            return 4;
        case UplinkDataType::Hold64:
        case UplinkDataType::HoldDouble:
        case UplinkDataType::Input64:
        case UplinkDataType::InputDouble:
            return 8;
    }
    return 0;
}

inline bool uplinkSigned(const ChannelConfig& config) {
    if (isBooleanType(config.dataType)) return false;
    if (isFloatingPointType(config.dataType)) return true;
    return config.signedValue;
}

inline bool setChannelName(ChannelConfig& config, const char* name, size_t length) {
    if (name == nullptr || length > kMaxChannelNameLength) return false;
    memset(config.name, 0, sizeof(config.name));
    if (length > 0) memcpy(config.name, name, length);
    config.name[length] = '\0';
    return true;
}

} // namespace modbus
} // namespace multibus
