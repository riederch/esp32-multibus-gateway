#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lorawan/FPort85ModbusUplink.h"

using multibus::lorawan::EncodeStatus;
using multibus::lorawan::FPort85ModbusUplink;
using multibus::modbus::ChannelConfig;
using multibus::modbus::DecodedScalar;
using multibus::modbus::ScalarKind;
using multibus::modbus::WireDataType;

static void testSigned16PeriodicValue() {
    ChannelConfig channel;
    channel.slot = 1;
    channel.slaveId = 1;
    channel.dataType = WireDataType::Input16AB;
    channel.quantity = 1;
    channel.signedValue = true;

    DecodedScalar value;
    value.kind = ScalarKind::SignedInteger;
    value.signedValue = -50;

    uint8_t encoded[32] = {0};
    size_t written = 0;
    assert(FPort85ModbusUplink::encodePollSuccess(channel, &value, 1, encoded, sizeof(encoded), written) == EncodeStatus::Ok);
    const uint8_t expected[] = {0xf9, 0x73, 0x01, 0x82, 0xce, 0xff};
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void testQuantityTwoUsesSecondRegisterFlag() {
    ChannelConfig channel;
    channel.slot = 0;
    channel.slaveId = 1;
    channel.dataType = WireDataType::Input32ABCD;
    channel.quantity = 2;
    channel.signedValue = false;

    DecodedScalar values[2];
    values[0].kind = ScalarKind::UnsignedInteger;
    values[0].unsignedValue = 0x00150020U;
    values[1].kind = ScalarKind::UnsignedInteger;
    values[1].unsignedValue = 0x00250030U;

    uint8_t encoded[32] = {0};
    size_t written = 0;
    assert(FPort85ModbusUplink::encodePollSuccess(channel, values, 2, encoded, sizeof(encoded), written) == EncodeStatus::Ok);

    const uint8_t expected[] = {
        0xf9, 0x73, 0x00, 0x06, 0x20, 0x00, 0x15, 0x00,
        0xf9, 0x73, 0x00, 0x26, 0x30, 0x00, 0x25, 0x00,
    };
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void testFloatIsLittleEndian() {
    ChannelConfig channel;
    channel.slot = 2;
    channel.slaveId = 1;
    channel.dataType = WireDataType::InputFloatABCD;
    channel.quantity = 1;

    DecodedScalar value;
    value.kind = ScalarKind::FloatingPoint;
    value.floatingValue = 5.0;

    uint8_t encoded[32] = {0};
    size_t written = 0;
    assert(FPort85ModbusUplink::encodePollSuccess(channel, &value, 1, encoded, sizeof(encoded), written) == EncodeStatus::Ok);
    const uint8_t expected[] = {0xf9, 0x73, 0x02, 0x87, 0x00, 0x00, 0xa0, 0x40};
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void testUpper16SignExtendsToFourByteContainer() {
    ChannelConfig channel;
    channel.slot = 3;
    channel.slaveId = 1;
    channel.dataType = WireDataType::Input32Upper16;
    channel.quantity = 1;
    channel.signedValue = true;

    DecodedScalar value;
    value.kind = ScalarKind::SignedInteger;
    value.signedValue = -2;

    uint8_t encoded[32] = {0};
    size_t written = 0;
    assert(FPort85ModbusUplink::encodePollSuccess(channel, &value, 1, encoded, sizeof(encoded), written) == EncodeStatus::Ok);
    const uint8_t expected[] = {0xf9, 0x73, 0x03, 0x88, 0xfe, 0xff, 0xff, 0xff};
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void testBooleanValue() {
    ChannelConfig channel;
    channel.slot = 4;
    channel.slaveId = 1;
    channel.dataType = WireDataType::Coil;
    channel.quantity = 1;

    DecodedScalar value;
    value.kind = ScalarKind::Boolean;
    value.booleanValue = true;

    uint8_t encoded[16] = {0};
    size_t written = 0;
    assert(FPort85ModbusUplink::encodePollSuccess(channel, &value, 1, encoded, sizeof(encoded), written) == EncodeStatus::Ok);
    const uint8_t expected[] = {0xf9, 0x73, 0x04, 0x00, 0x01};
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void testCollectionFailure() {
    ChannelConfig channel;
    channel.slot = 5;
    channel.slaveId = 1;
    channel.dataType = WireDataType::Input16AB;
    channel.quantity = 1;

    uint8_t encoded[8] = {0};
    size_t written = 0;
    assert(FPort85ModbusUplink::encodePollFailure(channel, encoded, sizeof(encoded), written) == EncodeStatus::Ok);
    const uint8_t expected[] = {0xff, 0x15, 0x05};
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void testRejectsMismatchedScalarKind() {
    ChannelConfig channel;
    channel.slot = 0;
    channel.slaveId = 1;
    channel.dataType = WireDataType::InputFloatABCD;
    channel.quantity = 1;

    DecodedScalar value;
    value.kind = ScalarKind::UnsignedInteger;
    value.unsignedValue = 5;

    uint8_t encoded[16] = {0};
    size_t written = 0;
    assert(FPort85ModbusUplink::encodePollSuccess(channel, &value, 1, encoded, sizeof(encoded), written) == EncodeStatus::Invalid);
    assert(written == 0);
}

int main() {
    testSigned16PeriodicValue();
    testQuantityTwoUsesSecondRegisterFlag();
    testFloatIsLittleEndian();
    testUpper16SignExtendsToFourByteContainer();
    testBooleanValue();
    testCollectionFailure();
    testRejectsMismatchedScalarKind();
    return 0;
}
