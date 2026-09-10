#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "modbus/ModbusChannel.h"

namespace multibus {
namespace lorawan {

constexpr uint8_t kCompatibilityFPort = 85;

struct CommandHeader {
    uint8_t channelId = 0;
    uint8_t type = 0;
};

enum class DecodeStatus : uint8_t {
    Ok,
    Truncated,
    Unsupported,
    Invalid,
};

enum class EncodeStatus : uint8_t {
    Ok,
    Invalid,
    BufferTooSmall,
};

enum class ModbusChannelOperation : uint8_t {
    Upsert,
    Remove,
    SetName,
};

struct ModbusChannelCommand {
    ModbusChannelOperation operation = ModbusChannelOperation::Upsert;
    modbus::ChannelConfig channel;
};

struct PeriodicValue {
    uint8_t slot = 0;
    modbus::UplinkDataType dataType = modbus::UplinkDataType::Coil;
    bool signedValue = false;
    uint8_t registerIndex = 0;
    const uint8_t* data = nullptr;
    size_t dataLength = 0;
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
    static constexpr uint8_t kCollectionExceptionType = 0x15;

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
        return (header.channelId == kModbusChannel && header.type == kModbusChannelDataType) ||
               (header.channelId == kSystemChannel && header.type == kCollectionExceptionType);
    }

    static DecodeStatus decodeModbusChannelCommand(const uint8_t* payload,
                                                    size_t length,
                                                    ModbusChannelCommand& command,
                                                    size_t& consumed) {
        consumed = 0;
        if (payload == nullptr || length < 3) return DecodeStatus::Truncated;
        if (payload[0] != kSystemChannel || payload[1] != kModbusChannelConfigType) {
            return DecodeStatus::Unsupported;
        }

        const uint8_t operation = payload[2];
        if (operation == 0x01) {
            if (length < 9) return DecodeStatus::Truncated;

            uint8_t slot = 0;
            if (!downlinkChannelIdToSlot(payload[3], slot)) return DecodeStatus::Invalid;
            if (payload[4] == 0 || payload[4] > 247) return DecodeStatus::Invalid;
            if (!modbus::validWireDataType(payload[7])) return DecodeStatus::Invalid;

            const uint8_t quantityAndSign = payload[8];
            if ((quantityAndSign & 0xE0U) != 0) return DecodeStatus::Invalid;
            const uint8_t quantity = quantityAndSign & 0x0FU;
            if (quantity < 1 || quantity > 2) return DecodeStatus::Invalid;

            command = ModbusChannelCommand{};
            command.operation = ModbusChannelOperation::Upsert;
            command.channel.slot = slot;
            command.channel.slaveId = payload[4];
            command.channel.address = static_cast<uint16_t>(payload[5]) |
                                      (static_cast<uint16_t>(payload[6]) << 8U);
            command.channel.dataType = static_cast<modbus::WireDataType>(payload[7]);
            command.channel.quantity = quantity;
            command.channel.signedValue = (quantityAndSign & 0x10U) != 0;
            consumed = 9;
            return modbus::validChannelConfig(command.channel) ? DecodeStatus::Ok : DecodeStatus::Invalid;
        }

        if (operation == 0x00) {
            if (length < 4) return DecodeStatus::Truncated;
            uint8_t slot = 0;
            if (!downlinkChannelIdToSlot(payload[3], slot)) return DecodeStatus::Invalid;
            command = ModbusChannelCommand{};
            command.operation = ModbusChannelOperation::Remove;
            command.channel.slot = slot;
            consumed = 4;
            return DecodeStatus::Ok;
        }

        if (operation == 0x02) {
            if (length < 5) return DecodeStatus::Truncated;
            uint8_t slot = 0;
            if (!downlinkChannelIdToSlot(payload[3], slot)) return DecodeStatus::Invalid;
            const size_t nameLength = payload[4];
            if (nameLength == 0 || nameLength > modbus::kMaxChannelNameLength) return DecodeStatus::Invalid;
            if (length < 5 + nameLength) return DecodeStatus::Truncated;

            command = ModbusChannelCommand{};
            command.operation = ModbusChannelOperation::SetName;
            command.channel.slot = slot;
            if (!modbus::setChannelName(command.channel,
                                        reinterpret_cast<const char*>(payload + 5),
                                        nameLength)) {
                return DecodeStatus::Invalid;
            }
            consumed = 5 + nameLength;
            return DecodeStatus::Ok;
        }

        return DecodeStatus::Unsupported;
    }

    static EncodeStatus encodePeriodicValue(const PeriodicValue& value,
                                            uint8_t* output,
                                            size_t capacity,
                                            size_t& written) {
        written = 0;
        if (value.slot >= modbus::kCompatibilitySlotCount || value.registerIndex > 1 ||
            value.data == nullptr) {
            return EncodeStatus::Invalid;
        }

        const size_t expected = modbus::uplinkValueWidth(value.dataType);
        if (expected == 0 || value.dataLength != expected) return EncodeStatus::Invalid;
        if (capacity < 4 + expected || output == nullptr) return EncodeStatus::BufferTooSmall;

        output[0] = kModbusChannel;
        output[1] = kModbusChannelDataType;
        output[2] = value.slot;
        output[3] = static_cast<uint8_t>((value.signedValue ? 0x80U : 0x00U) |
                                         (value.registerIndex == 1 ? 0x20U : 0x00U) |
                                         (static_cast<uint8_t>(value.dataType) & 0x1FU));
        memcpy(output + 4, value.data, expected);
        written = 4 + expected;
        return EncodeStatus::Ok;
    }

    static EncodeStatus encodePeriodicValue(const modbus::ChannelConfig& channel,
                                            uint8_t registerIndex,
                                            const uint8_t* data,
                                            size_t dataLength,
                                            uint8_t* output,
                                            size_t capacity,
                                            size_t& written) {
        if (!modbus::validChannelConfig(channel)) return EncodeStatus::Invalid;
        PeriodicValue value;
        value.slot = channel.slot;
        value.dataType = modbus::uplinkDataType(channel.dataType);
        value.signedValue = modbus::uplinkSigned(channel);
        value.registerIndex = registerIndex;
        value.data = data;
        value.dataLength = dataLength;
        return encodePeriodicValue(value, output, capacity, written);
    }

    static EncodeStatus encodeCollectionException(uint8_t slot,
                                                  uint8_t* output,
                                                  size_t capacity,
                                                  size_t& written) {
        written = 0;
        if (slot >= modbus::kCompatibilitySlotCount) return EncodeStatus::Invalid;
        if (output == nullptr || capacity < 3) return EncodeStatus::BufferTooSmall;
        output[0] = kSystemChannel;
        output[1] = kCollectionExceptionType;
        output[2] = slot;
        written = 3;
        return EncodeStatus::Ok;
    }

    static bool downlinkChannelIdToSlot(uint8_t channelId, uint8_t& slot) {
        if (channelId < 1 || channelId > modbus::kCompatibilitySlotCount) return false;
        slot = static_cast<uint8_t>(channelId - 1);
        return true;
    }

    static uint8_t slotToDownlinkChannelId(uint8_t slot) {
        return slot < modbus::kCompatibilitySlotCount ? static_cast<uint8_t>(slot + 1) : 0;
    }
};

} // namespace lorawan
} // namespace multibus
