#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lorawan/FPort85Alarm.h"

using multibus::lorawan::AlarmKind;
using multibus::lorawan::EncodeStatus;
using multibus::lorawan::FPort85Alarm;
using multibus::modbus::ChannelConfig;
using multibus::modbus::DecodedScalar;
using multibus::modbus::ScalarKind;
using multibus::modbus::WireDataType;

static void testThresholdReferenceVector() {
    ChannelConfig channel;
    channel.slot = 15;
    channel.slaveId = 1;
    channel.dataType = WireDataType::HoldFloatABCD;
    channel.quantity = 1;
    channel.signedValue = true;

    DecodedScalar value;
    value.kind = ScalarKind::FloatingPoint;
    value.floatingValue = -2580.0;

    uint8_t encoded[32] = {0};
    size_t written = 0;
    assert(FPort85Alarm::encode(
        channel, 0, value, AlarmKind::Threshold, 0.0,
        encoded, sizeof(encoded), written) == EncodeStatus::Ok);

    const uint8_t expected[] = {
        0xf9, 0x73, 0x4f, 0x85,
        0x00, 0x40, 0x21, 0xc5
    };
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void testThresholdReleaseReferenceShape() {
    ChannelConfig channel;
    channel.slot = 31;
    channel.slaveId = 1;
    channel.dataType = WireDataType::InputDoubleABCDEFGH;
    channel.quantity = 1;
    channel.signedValue = true;

    DecodedScalar value;
    value.kind = ScalarKind::FloatingPoint;
    value.floatingValue = -9999.55;

    uint8_t encoded[32] = {0};
    size_t written = 0;
    assert(FPort85Alarm::encode(
        channel, 0, value, AlarmKind::ThresholdRelease, 0.0,
        encoded, sizeof(encoded), written) == EncodeStatus::Ok);
    assert(encoded[0] == 0xf9);
    assert(encoded[1] == 0x73);
    assert(encoded[2] == 0x9f);
    assert(encoded[3] == 0x8f);
    assert(written == 12);
}

static void testChangeReferenceVector() {
    ChannelConfig channel;
    channel.slot = 0;
    channel.slaveId = 1;
    channel.dataType = WireDataType::Hold16AB;
    channel.quantity = 1;
    channel.signedValue = false;

    DecodedScalar value;
    value.kind = ScalarKind::UnsignedInteger;
    value.unsignedValue = 15;

    uint8_t encoded[32] = {0};
    size_t written = 0;
    assert(FPort85Alarm::encode(
        channel, 0, value, AlarmKind::Change, 10.0,
        encoded, sizeof(encoded), written) == EncodeStatus::Ok);

    const uint8_t expected[] = {
        0xf9, 0x73, 0xc0, 0x03, 0x0f, 0x00,
        0xf9, 0x74, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x40
    };
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void testBooleanAlarmRejected() {
    ChannelConfig channel;
    channel.slot = 0;
    channel.slaveId = 1;
    channel.dataType = WireDataType::Coil;
    channel.quantity = 1;

    DecodedScalar value;
    value.kind = ScalarKind::Boolean;
    value.booleanValue = true;

    uint8_t encoded[16] = {0};
    size_t written = 0;
    assert(FPort85Alarm::encode(
        channel, 0, value, AlarmKind::Threshold, 0.0,
        encoded, sizeof(encoded), written) == EncodeStatus::Invalid);
}

int main() {
    testThresholdReferenceVector();
    testThresholdReleaseReferenceShape();
    testChangeReferenceVector();
    testBooleanAlarmRejected();
    return 0;
}
