#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lorawan/FPort85History.h"

using multibus::lorawan::EncodeStatus;
using multibus::lorawan::FPort85History;
using multibus::lorawan::HistoricalRecord;
using multibus::lorawan::HistoryRing;
using multibus::lorawan::kHistoricalModbusRecordLength;
using multibus::modbus::ChannelConfig;
using multibus::modbus::DecodedScalar;
using multibus::modbus::ScalarKind;
using multibus::modbus::WireDataType;

static void testHistoricalReferenceVector() {
    ChannelConfig channel;
    channel.slot = 1;
    channel.slaveId = 1;
    channel.dataType = WireDataType::Input16AB;
    channel.quantity = 2;
    channel.signedValue = true;

    DecodedScalar values[2];
    values[0].kind = ScalarKind::SignedInteger;
    values[0].signedValue = 20;
    values[1].kind = ScalarKind::SignedInteger;
    values[1].signedValue = -35;

    uint8_t encoded[kHistoricalModbusRecordLength] = {0};
    size_t written = 0;
    assert(FPort85History::encodeModbusRecord(
        1666938125U,
        channel,
        true,
        values,
        2,
        encoded,
        sizeof(encoded),
        written) == EncodeStatus::Ok);

    const uint8_t expected[] = {
        0x21, 0xce,
        0x0d, 0x75, 0x5b, 0x63,
        0x01,
        0x80, 0x85,
        0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xdd, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void testHistoricalFailureHasNoValues() {
    ChannelConfig channel;
    channel.slot = 4;
    channel.slaveId = 2;
    channel.dataType = WireDataType::Input32ABCD;
    channel.quantity = 1;

    uint8_t encoded[kHistoricalModbusRecordLength] = {0};
    size_t written = 0;
    assert(FPort85History::encodeModbusRecord(
        1234U,
        channel,
        false,
        nullptr,
        0,
        encoded,
        sizeof(encoded),
        written) == EncodeStatus::Ok);
    assert(written == kHistoricalModbusRecordLength);
    assert(encoded[6] == 4);
    assert((encoded[8] & 0x01U) == 0); // fetch-success bit is clear
    for (size_t i = 9; i < kHistoricalModbusRecordLength; ++i) {
        assert(encoded[i] == 0);
    }
}

static HistoricalRecord makeRecord(uint32_t timestamp) {
    HistoricalRecord record;
    record.payload[0] = 0x21;
    record.payload[1] = 0xce;
    record.payload[2] = static_cast<uint8_t>(timestamp & 0xffU);
    record.payload[3] = static_cast<uint8_t>((timestamp >> 8U) & 0xffU);
    record.payload[4] = static_cast<uint8_t>((timestamp >> 16U) & 0xffU);
    record.payload[5] = static_cast<uint8_t>((timestamp >> 24U) & 0xffU);
    return record;
}

static void testBoundedRingOverwritesOldest() {
    HistoryRing<2> ring;
    ring.push(makeRecord(10));
    ring.push(makeRecord(20));
    assert(ring.size() == 2);
    assert(ring.oldest(0)->timestamp() == 10);
    assert(ring.oldest(1)->timestamp() == 20);

    ring.push(makeRecord(30));
    assert(ring.size() == 2);
    assert(ring.oldest(0)->timestamp() == 20);
    assert(ring.oldest(1)->timestamp() == 30);
    assert(ring.oldest(2) == nullptr);
}

int main() {
    testHistoricalReferenceVector();
    testHistoricalFailureHasNoValues();
    testBoundedRingOverwritesOldest();
    return 0;
}
