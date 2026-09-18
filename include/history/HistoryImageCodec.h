#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lorawan/FPort85History.h"

namespace multibus::history {

constexpr size_t kPersistentHistoryCapacity = 64;

template <size_t Capacity>
class HistoryImageCodec {
public:
    static constexpr size_t kHeaderLength = 12;
    static constexpr size_t kImageLength =
        kHeaderLength + Capacity * lorawan::kHistoricalModbusRecordLength + 4;

    static bool encode(const lorawan::HistoryRing<Capacity>& ring,
                       uint32_t generation,
                       uint8_t* output,
                       size_t capacity,
                       size_t& written) {
        written = 0;
        if (output == nullptr || capacity < kImageLength || ring.size() > Capacity) return false;

        memset(output, 0, kImageLength);
        output[0] = 'M';
        output[1] = 'B';
        output[2] = 'H';
        output[3] = 1;
        writeU32(generation, output + 4);
        writeU16(static_cast<uint16_t>(ring.size()), output + 8);

        for (size_t i = 0; i < ring.size(); ++i) {
            const lorawan::HistoricalRecord* record = ring.oldest(i);
            if (record == nullptr) return false;
            memcpy(output + kHeaderLength + i * lorawan::kHistoricalModbusRecordLength,
                   record->payload,
                   lorawan::kHistoricalModbusRecordLength);
        }

        const uint32_t crc = crc32(output, kImageLength - 4);
        writeU32(crc, output + kImageLength - 4);
        written = kImageLength;
        return true;
    }

    static bool decode(const uint8_t* input,
                       size_t length,
                       lorawan::HistoryRing<Capacity>& ring,
                       uint32_t& generation) {
        if (input == nullptr || length != kImageLength) return false;
        if (input[0] != 'M' || input[1] != 'B' || input[2] != 'H' || input[3] != 1) return false;

        const uint32_t storedCrc = readU32(input + kImageLength - 4);
        if (crc32(input, kImageLength - 4) != storedCrc) return false;

        const uint16_t count = readU16(input + 8);
        if (count > Capacity) return false;

        lorawan::HistoryRing<Capacity> decoded;
        for (uint16_t i = 0; i < count; ++i) {
            lorawan::HistoricalRecord record;
            memcpy(record.payload,
                   input + kHeaderLength + static_cast<size_t>(i) * lorawan::kHistoricalModbusRecordLength,
                   lorawan::kHistoricalModbusRecordLength);
            if (record.payload[0] != 0x21 || record.payload[1] != 0xce) return false;
            decoded.push(record);
        }

        generation = readU32(input + 4);
        ring = decoded;
        return true;
    }

private:
    static uint32_t crc32(const uint8_t* data, size_t length) {
        uint32_t crc = 0xffffffffU;
        for (size_t i = 0; i < length; ++i) {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1U) ^ (0xedb88320U & static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1U))));
            }
        }
        return ~crc;
    }

    static void writeU16(uint16_t value, uint8_t* output) {
        output[0] = static_cast<uint8_t>(value & 0xffU);
        output[1] = static_cast<uint8_t>((value >> 8U) & 0xffU);
    }

    static void writeU32(uint32_t value, uint8_t* output) {
        output[0] = static_cast<uint8_t>(value & 0xffU);
        output[1] = static_cast<uint8_t>((value >> 8U) & 0xffU);
        output[2] = static_cast<uint8_t>((value >> 16U) & 0xffU);
        output[3] = static_cast<uint8_t>((value >> 24U) & 0xffU);
    }

    static uint16_t readU16(const uint8_t* input) {
        return static_cast<uint16_t>(input[0]) |
               (static_cast<uint16_t>(input[1]) << 8U);
    }

    static uint32_t readU32(const uint8_t* input) {
        return static_cast<uint32_t>(input[0]) |
               (static_cast<uint32_t>(input[1]) << 8U) |
               (static_cast<uint32_t>(input[2]) << 16U) |
               (static_cast<uint32_t>(input[3]) << 24U);
    }
};

} // namespace multibus::history
