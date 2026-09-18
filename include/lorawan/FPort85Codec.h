#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "modbus/ModbusChannel.h"
#include "modbus/ModbusMasterSettings.h"
#include "modbus/Rs485Settings.h"

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

struct Rs485SettingsCommand {
    modbus::Rs485SerialSettings settings;
};

enum class BasicControlCommand : uint8_t {
    Rejoin,
    Reboot,
};

struct PeriodicReportEnquiryCommand {};

struct UtcTimezoneCommand {
    int16_t offsetMinutes = 0;
};

struct LnsTimeSyncCommand {};

struct HistoryToggleCommand {
    bool enabled = false;
};

struct RetransmissionIntervalCommand {
    uint16_t seconds = 600;
};

struct DstSettingsCommand {
    bool enabled = false;
    uint8_t biasMinutes = 0;
    uint8_t startMonth = 0;
    uint8_t startWeek = 0;
    uint8_t startWeekday = 0;
    uint16_t startMinuteOfDay = 0;
    uint8_t endMonth = 0;
    uint8_t endWeek = 0;
    uint8_t endWeekday = 0;
    uint16_t endMinuteOfDay = 0;
};

struct ModbusMasterSettingsCommand {
    modbus::ModbusMasterSettings settings;
};

enum class Rs485SettingsEnquiryKind : uint8_t {
    Serial = 0x00,
    Modbus = 0x01,
};

struct Rs485SettingsEnquiryCommand {
    Rs485SettingsEnquiryKind kind = Rs485SettingsEnquiryKind::Serial;
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
    static constexpr uint8_t kRejoinType = 0x04;
    static constexpr uint8_t kRebootType = 0x10;
    static constexpr uint8_t kPeriodicReportEnquiryType = 0x28;
    static constexpr uint8_t kUtcTimezoneType = 0xBD;
    static constexpr uint8_t kLnsTimeSyncType = 0x4A;
    static constexpr uint8_t kDstSettingsType = 0x72;
    static constexpr uint8_t kDataStorageType = 0x68;
    static constexpr uint8_t kDataRetransmissionType = 0x69;
    static constexpr uint8_t kRetransmissionIntervalType = 0x0D;
    static constexpr uint8_t kRs485ConfigType = 0x78;
    static constexpr uint8_t kModbusGlobalConfigType = 0x79;
    static constexpr uint8_t kRs485SettingsEnquiryType = 0x7A;
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
        if (header.channelId == kSystemChannel && header.type == kRejoinType) return true;
        if (header.channelId == kSystemChannel && header.type == kRebootType) return true;
        if (header.channelId == kSystemChannel && header.type == kPeriodicReportEnquiryType) return true;
        if (header.channelId == kSystemChannel && header.type == kUtcTimezoneType) return true;
        if (header.channelId == kSystemChannel && header.type == kLnsTimeSyncType) return true;
        if (header.channelId == kModbusChannel && header.type == kDstSettingsType) return true;
        if (header.channelId == kSystemChannel && header.type == kDataStorageType) return true;
        if (header.channelId == kSystemChannel && header.type == kDataRetransmissionType) return true;
        if (header.channelId == kModbusChannel && header.type == kRetransmissionIntervalType) return true;
        if (header.channelId == kModbusChannel && header.type == kRs485ConfigType) return true;
        if (header.channelId == kModbusChannel && header.type == kModbusGlobalConfigType) return true;
        if (header.channelId == kModbusChannel && header.type == kRs485SettingsEnquiryType) return true;
        if (header.channelId == kSystemChannel && header.type == kModbusChannelConfigType) return true;
        return false;
    }

    static bool isVerifiedUplinkPrefix(const CommandHeader& header) {
        return (header.channelId == kModbusChannel && header.type == kModbusChannelDataType) ||
               (header.channelId == kSystemChannel && header.type == kCollectionExceptionType);
    }

    static DecodeStatus decodeBasicControlCommand(const uint8_t* payload,
                                                  size_t length,
                                                  BasicControlCommand& command,
                                                  size_t& consumed) {
        consumed = 0;
        if (payload == nullptr || length < 3) return DecodeStatus::Truncated;
        if (payload[0] != kSystemChannel) return DecodeStatus::Unsupported;
        if (payload[2] != 0xff) return DecodeStatus::Invalid;

        if (payload[1] == kRejoinType) {
            command = BasicControlCommand::Rejoin;
        } else if (payload[1] == kRebootType) {
            command = BasicControlCommand::Reboot;
        } else {
            return DecodeStatus::Unsupported;
        }

        consumed = 3;
        return DecodeStatus::Ok;
    }

    static DecodeStatus decodePeriodicReportEnquiryCommand(
        const uint8_t* payload,
        size_t length,
        PeriodicReportEnquiryCommand& command,
        size_t& consumed) {
        consumed = 0;
        if (payload == nullptr || length < 3) return DecodeStatus::Truncated;
        if (payload[0] != kSystemChannel || payload[1] != kPeriodicReportEnquiryType) {
            return DecodeStatus::Unsupported;
        }
        if (payload[2] != 0xff) return DecodeStatus::Invalid;

        command = PeriodicReportEnquiryCommand{};
        consumed = 3;
        return DecodeStatus::Ok;
    }

    static DecodeStatus decodeUtcTimezoneCommand(const uint8_t* payload,
                                                 size_t length,
                                                 UtcTimezoneCommand& command,
                                                 size_t& consumed) {
        consumed = 0;
        if (payload == nullptr || length < 4) return DecodeStatus::Truncated;
        if (payload[0] != kSystemChannel || payload[1] != kUtcTimezoneType) {
            return DecodeStatus::Unsupported;
        }

        const uint16_t raw = static_cast<uint16_t>(payload[2]) |
                             (static_cast<uint16_t>(payload[3]) << 8U);
        const int16_t minutes = static_cast<int16_t>(raw);
        if (minutes < -720 || minutes > 840) {
            return DecodeStatus::Invalid;
        }

        command = UtcTimezoneCommand{};
        command.offsetMinutes = minutes;
        consumed = 4;
        return DecodeStatus::Ok;
    }

    static DecodeStatus decodeLnsTimeSyncCommand(const uint8_t* payload,
                                                size_t length,
                                                LnsTimeSyncCommand& command,
                                                size_t& consumed) {
        consumed = 0;
        if (payload == nullptr || length < 3) return DecodeStatus::Truncated;
        if (payload[0] != kSystemChannel || payload[1] != kLnsTimeSyncType) {
            return DecodeStatus::Unsupported;
        }
        if (payload[2] != 0x00) return DecodeStatus::Invalid;

        command = LnsTimeSyncCommand{};
        consumed = 3;
        return DecodeStatus::Ok;
    }

    static DecodeStatus decodeDstSettingsCommand(const uint8_t* payload,
                                               size_t length,
                                               DstSettingsCommand& command,
                                               size_t& consumed) {
        consumed = 0;
        if (payload == nullptr || length < 11) return DecodeStatus::Truncated;
        if (payload[0] != kModbusChannel || payload[1] != kDstSettingsType) {
            return DecodeStatus::Unsupported;
        }

        command = DstSettingsCommand{};
        command.enabled = (payload[2] & 0x80U) != 0;
        command.biasMinutes = payload[2] & 0x7fU;
        command.startMonth = payload[3];
        command.startWeek = (payload[4] >> 4U) & 0x0fU;
        command.startWeekday = payload[4] & 0x0fU;
        command.startMinuteOfDay = static_cast<uint16_t>(payload[5]) |
                                   (static_cast<uint16_t>(payload[6]) << 8U);
        command.endMonth = payload[7];
        command.endWeek = (payload[8] >> 4U) & 0x0fU;
        command.endWeekday = payload[8] & 0x0fU;
        command.endMinuteOfDay = static_cast<uint16_t>(payload[9]) |
                                 (static_cast<uint16_t>(payload[10]) << 8U);

        if (command.biasMinutes > 120) return DecodeStatus::Invalid;
        if (command.enabled) {
            if (command.biasMinutes == 0 ||
                command.startMonth < 1 || command.startMonth > 12 ||
                command.startWeek < 1 || command.startWeek > 5 ||
                command.startWeekday < 1 || command.startWeekday > 7 ||
                command.startMinuteOfDay > 1439 ||
                command.endMonth < 1 || command.endMonth > 12 ||
                command.endWeek < 1 || command.endWeek > 5 ||
                command.endWeekday < 1 || command.endWeekday > 7 ||
                command.endMinuteOfDay > 1439) {
                return DecodeStatus::Invalid;
            }
        }

        consumed = 11;
        return DecodeStatus::Ok;
    }

    static DecodeStatus decodeHistoryToggleCommand(const uint8_t* payload,
                                                    size_t length,
                                                    uint8_t expectedType,
                                                    HistoryToggleCommand& command,
                                                    size_t& consumed) {
        consumed = 0;
        if (payload == nullptr || length < 3) return DecodeStatus::Truncated;
        if (payload[0] != kSystemChannel || payload[1] != expectedType) {
            return DecodeStatus::Unsupported;
        }
        if (expectedType != kDataStorageType && expectedType != kDataRetransmissionType) {
            return DecodeStatus::Unsupported;
        }
        if (payload[2] > 0x01U) return DecodeStatus::Invalid;

        command = HistoryToggleCommand{};
        command.enabled = payload[2] == 0x01U;
        consumed = 3;
        return DecodeStatus::Ok;
    }

    static DecodeStatus decodeRetransmissionIntervalCommand(
        const uint8_t* payload,
        size_t length,
        RetransmissionIntervalCommand& command,
        size_t& consumed) {
        consumed = 0;
        if (payload == nullptr || length < 4) return DecodeStatus::Truncated;
        if (payload[0] != kModbusChannel || payload[1] != kRetransmissionIntervalType) {
            return DecodeStatus::Unsupported;
        }

        const uint16_t seconds = static_cast<uint16_t>(payload[2]) |
                                 (static_cast<uint16_t>(payload[3]) << 8U);
        if (seconds < 30 || seconds > 1200) return DecodeStatus::Invalid;

        command = RetransmissionIntervalCommand{};
        command.seconds = seconds;
        consumed = 4;
        return DecodeStatus::Ok;
    }

    static DecodeStatus decodeRs485SettingsCommand(const uint8_t* payload,
                                                    size_t length,
                                                    Rs485SettingsCommand& command,
                                                    size_t& consumed) {
        consumed = 0;
        if (payload == nullptr || length < 9) return DecodeStatus::Truncated;
        if (payload[0] != kModbusChannel || payload[1] != kRs485ConfigType) {
            return DecodeStatus::Unsupported;
        }

        modbus::Rs485SerialSettings settings;
        settings.baudRate = static_cast<uint32_t>(payload[2]) |
                            (static_cast<uint32_t>(payload[3]) << 8U) |
                            (static_cast<uint32_t>(payload[4]) << 16U) |
                            (static_cast<uint32_t>(payload[5]) << 24U);
        settings.dataBits = payload[6];
        settings.stopBits = static_cast<modbus::Rs485StopBits>(payload[7]);
        settings.parity = static_cast<modbus::Rs485Parity>(payload[8]);
        if (!modbus::validRs485SerialSettings(settings)) return DecodeStatus::Invalid;

        command = Rs485SettingsCommand{};
        command.settings = settings;
        consumed = 9;
        return DecodeStatus::Ok;
    }

    static DecodeStatus decodeModbusMasterSettingsCommand(const uint8_t* payload,
                                                           size_t length,
                                                           ModbusMasterSettingsCommand& command,
                                                           size_t& consumed) {
        consumed = 0;
        if (payload == nullptr || length < 9) return DecodeStatus::Truncated;
        if (payload[0] != kModbusChannel || payload[1] != kModbusGlobalConfigType) {
            return DecodeStatus::Unsupported;
        }

        modbus::ModbusMasterSettings settings;
        settings.executionIntervalMs = static_cast<uint16_t>(payload[2]) |
                                       (static_cast<uint16_t>(payload[3]) << 8U);
        settings.maxResponseTimeMs = static_cast<uint16_t>(payload[4]) |
                                     (static_cast<uint16_t>(payload[5]) << 8U);
        settings.maxRetryTimes = payload[6];
        settings.passThroughMode = static_cast<modbus::PassThroughMode>(payload[7]);
        settings.passThroughPort = payload[8];
        if (!modbus::validModbusMasterSettings(settings)) return DecodeStatus::Invalid;

        command = ModbusMasterSettingsCommand{};
        command.settings = settings;
        consumed = 9;
        return DecodeStatus::Ok;
    }

    static DecodeStatus decodeRs485SettingsEnquiryCommand(
        const uint8_t* payload,
        size_t length,
        Rs485SettingsEnquiryCommand& command,
        size_t& consumed) {
        consumed = 0;
        if (payload == nullptr || length < 3) return DecodeStatus::Truncated;
        if (payload[0] != kModbusChannel || payload[1] != kRs485SettingsEnquiryType) {
            return DecodeStatus::Unsupported;
        }
        if (payload[2] > static_cast<uint8_t>(Rs485SettingsEnquiryKind::Modbus)) {
            return DecodeStatus::Invalid;
        }

        command = Rs485SettingsEnquiryCommand{};
        command.kind = static_cast<Rs485SettingsEnquiryKind>(payload[2]);
        consumed = 3;
        return DecodeStatus::Ok;
    }

    static EncodeStatus encodeRs485SettingsEnquiryReply(
        const Rs485SettingsEnquiryCommand& command,
        const modbus::Rs485SerialSettings& serialSettings,
        const modbus::ModbusMasterSettings& masterSettings,
        uint8_t* output,
        size_t capacity,
        size_t& written) {
        written = 0;
        if (output == nullptr) return EncodeStatus::Invalid;

        const bool serial = command.kind == Rs485SettingsEnquiryKind::Serial;
        const bool modbus = command.kind == Rs485SettingsEnquiryKind::Modbus;
        if (!serial && !modbus) return EncodeStatus::Invalid;

        const size_t settingLength = serial ? 9U : 9U;
        const size_t totalLength = 4U + settingLength;
        if (capacity < totalLength) return EncodeStatus::BufferTooSmall;

        output[0] = 0xF8;
        output[1] = kRs485SettingsEnquiryType;
        output[2] = static_cast<uint8_t>(command.kind);
        output[3] = 0x00;

        if (serial) {
            if (!modbus::validRs485SerialSettings(serialSettings)) return EncodeStatus::Invalid;
            output[4] = kModbusChannel;
            output[5] = kRs485ConfigType;
            output[6] = static_cast<uint8_t>(serialSettings.baudRate & 0xffU);
            output[7] = static_cast<uint8_t>((serialSettings.baudRate >> 8U) & 0xffU);
            output[8] = static_cast<uint8_t>((serialSettings.baudRate >> 16U) & 0xffU);
            output[9] = static_cast<uint8_t>((serialSettings.baudRate >> 24U) & 0xffU);
            output[10] = serialSettings.dataBits;
            output[11] = static_cast<uint8_t>(serialSettings.stopBits);
            output[12] = static_cast<uint8_t>(serialSettings.parity);
        } else {
            if (!modbus::validModbusMasterSettings(masterSettings)) return EncodeStatus::Invalid;
            output[4] = kModbusChannel;
            output[5] = kModbusGlobalConfigType;
            output[6] = static_cast<uint8_t>(masterSettings.executionIntervalMs & 0xffU);
            output[7] = static_cast<uint8_t>((masterSettings.executionIntervalMs >> 8U) & 0xffU);
            output[8] = static_cast<uint8_t>(masterSettings.maxResponseTimeMs & 0xffU);
            output[9] = static_cast<uint8_t>((masterSettings.maxResponseTimeMs >> 8U) & 0xffU);
            output[10] = masterSettings.maxRetryTimes;
            output[11] = static_cast<uint8_t>(masterSettings.passThroughMode);
            output[12] = masterSettings.passThroughPort;
        }

        written = totalLength;
        return EncodeStatus::Ok;
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
