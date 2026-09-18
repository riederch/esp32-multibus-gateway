#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FPort85ModbusUplink.h"

namespace multibus::lorawan {

constexpr size_t kHistoricalModbusRecordLength = 25;

class FPort85History {
public:
    static EncodeStatus encodeModbusRecord(uint32_t unixTimestamp,
                                           const modbus::ChannelConfig& channel,
                                           bool success,
                                           const modbus::DecodedScalar* values,
                                           uint8_t valueCount,
                                           uint8_t* output,
                                           size_t capacity,
                                           size_t& written) {
        written = 0;
        if (!modbus::validChannelConfig(channel) || output == nullptr ||
            capacity < kHistoricalModbusRecordLength) {
            return output == nullptr || !modbus::validChannelConfig(channel)
                ? EncodeStatus::Invalid
                : EncodeStatus::BufferTooSmall;
        }

        if (success) {
            if (values == nullptr || valueCount != channel.quantity || valueCount == 0 || valueCount > 2) {
                return EncodeStatus::Invalid;
            }
        } else if (valueCount != 0) {
            return EncodeStatus::Invalid;
        }

        memset(output, 0, kHistoricalModbusRecordLength);
        output[0] = 0x21;
        output[1] = 0xce;
        writeU32(unixTimestamp, output + 2);
        output[6] = channel.slot;

        uint16_t control = 0;
        if (modbus::uplinkSigned(channel)) control |= 0x8000U;
        control |= static_cast<uint16_t>(
            (static_cast<uint16_t>(static_cast<uint8_t>(channel.dataType)) & 0x3fU) << 9U);
        if (success) control |= 0x0100U;
        control |= static_cast<uint16_t>((channel.quantity & 0x03U) << 6U);
        output[7] = static_cast<uint8_t>(control & 0xffU);
        output[8] = static_cast<uint8_t>((control >> 8U) & 0xffU);

        if (success) {
            for (uint8_t index = 0; index < valueCount; ++index) {
                uint8_t encoded[8] = {0};
                size_t encodedLength = 0;
                const EncodeStatus status = FPort85ModbusUplink::encodeScalarBytes(
                    channel, values[index], encoded, sizeof(encoded), encodedLength);
                if (status != EncodeStatus::Ok) return status;
                memcpy(output + 9U + static_cast<size_t>(index) * 8U, encoded, encodedLength);
            }
        }

        written = kHistoricalModbusRecordLength;
        return EncodeStatus::Ok;
    }

private:
    static void writeU32(uint32_t value, uint8_t* output) {
        output[0] = static_cast<uint8_t>(value & 0xffU);
        output[1] = static_cast<uint8_t>((value >> 8U) & 0xffU);
        output[2] = static_cast<uint8_t>((value >> 16U) & 0xffU);
        output[3] = static_cast<uint8_t>((value >> 24U) & 0xffU);
    }
};

struct HistoricalRecord {
    uint8_t payload[kHistoricalModbusRecordLength] = {0};

    uint32_t timestamp() const {
        return static_cast<uint32_t>(payload[2]) |
               (static_cast<uint32_t>(payload[3]) << 8U) |
               (static_cast<uint32_t>(payload[4]) << 16U) |
               (static_cast<uint32_t>(payload[5]) << 24U);
    }
};

template <size_t Capacity>
class HistoryRing {
public:
    static_assert(Capacity > 0, "HistoryRing capacity must be greater than zero");

    void clear() {
        start_ = 0;
        size_ = 0;
    }

    size_t size() const { return size_; }
    constexpr size_t capacity() const { return Capacity; }
    bool empty() const { return size_ == 0; }

    void push(const HistoricalRecord& record) {
        if (size_ < Capacity) {
            records_[(start_ + size_) % Capacity] = record;
            ++size_;
            return;
        }

        records_[start_] = record;
        start_ = (start_ + 1U) % Capacity;
    }

    const HistoricalRecord* oldest(size_t index) const {
        if (index >= size_) return nullptr;
        return &records_[(start_ + index) % Capacity];
    }

private:
    HistoricalRecord records_[Capacity];
    size_t start_ = 0;
    size_t size_ = 0;
};

} // namespace multibus::lorawan
