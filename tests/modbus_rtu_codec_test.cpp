#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "modbus/ModbusRtuCodec.h"

using multibus::modbus::ChannelConfig;
using multibus::modbus::DecodedScalar;
using multibus::modbus::ModbusRtuCodec;
using multibus::modbus::RtuDecodeStatus;
using multibus::modbus::ScalarKind;
using multibus::modbus::WireDataType;

static size_t withCrc(uint8_t* frame, size_t payloadLength) {
    const uint16_t crc = ModbusRtuCodec::crc16(frame, payloadLength);
    frame[payloadLength] = static_cast<uint8_t>(crc & 0xffU);
    frame[payloadLength + 1] = static_cast<uint8_t>((crc >> 8U) & 0xffU);
    return payloadLength + 2;
}

static ChannelConfig channel(WireDataType type, uint8_t quantity = 1) {
    ChannelConfig config;
    config.slot = 0;
    config.slaveId = 1;
    config.address = 0;
    config.dataType = type;
    config.quantity = quantity;
    return config;
}

static void testKnownCrcVector() {
    const uint8_t payload[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0a};
    assert(ModbusRtuCodec::crc16(payload, sizeof(payload)) == 0xcdc5U);
}

static void testBuildHoldingRegisterRead() {
    ChannelConfig config = channel(WireDataType::Hold16AB, 2);
    config.address = 0x006b;

    uint8_t request[8] = {0};
    size_t written = 0;
    assert(ModbusRtuCodec::buildReadRequest(config, request, sizeof(request), written) == RtuDecodeStatus::Ok);
    assert(written == 8);

    const uint8_t expected[] = {0x01, 0x03, 0x00, 0x6b, 0x00, 0x02, 0xb5, 0xd7};
    assert(memcmp(request, expected, sizeof(expected)) == 0);
}

static void testBuildTwoInt32ValuesReadsFourRegisters() {
    ChannelConfig config = channel(WireDataType::Input32ABCD, 2);
    config.slaveId = 0x11;
    config.address = 0x006b;

    uint8_t request[8] = {0};
    size_t written = 0;
    assert(ModbusRtuCodec::readQuantity(config) == 4);
    assert(ModbusRtuCodec::buildReadRequest(config, request, sizeof(request), written) == RtuDecodeStatus::Ok);

    const uint8_t expected[] = {0x11, 0x04, 0x00, 0x6b, 0x00, 0x04, 0x82, 0x85};
    assert(memcmp(request, expected, sizeof(expected)) == 0);
}

static void testDecodeTwoSignedInt16Values() {
    ChannelConfig config = channel(WireDataType::Input16AB, 2);
    config.signedValue = true;

    uint8_t response[9] = {0x01, 0x04, 0x04, 0x00, 0x15, 0xff, 0xce, 0x00, 0x00};
    const size_t length = withCrc(response, 7);

    DecodedScalar values[2];
    uint8_t count = 0;
    uint8_t exception = 0;
    assert(ModbusRtuCodec::decodeReadResponse(config, response, length, values, count, exception) == RtuDecodeStatus::Ok);
    assert(count == 2);
    assert(values[0].kind == ScalarKind::SignedInteger);
    assert(values[0].signedValue == 21);
    assert(values[1].signedValue == -50);
}

static void testDecodeInt32ByteOrders() {
    uint8_t response[9] = {0x01, 0x04, 0x04, 0x00, 0x15, 0x00, 0x20, 0x00, 0x00};
    const size_t length = withCrc(response, 7);

    ChannelConfig config = channel(WireDataType::Input32CDAB);
    DecodedScalar values[2];
    uint8_t count = 0;
    uint8_t exception = 0;
    assert(ModbusRtuCodec::decodeReadResponse(config, response, length, values, count, exception) == RtuDecodeStatus::Ok);
    assert(count == 1);
    assert(values[0].kind == ScalarKind::UnsignedInteger);
    assert(values[0].unsignedValue == 0x00200015ULL);
}

static void testDecodeFloat() {
    uint8_t response[9] = {0x01, 0x04, 0x04, 0x40, 0xa0, 0x00, 0x00, 0x00, 0x00};
    const size_t length = withCrc(response, 7);

    ChannelConfig config = channel(WireDataType::InputFloatABCD);
    DecodedScalar values[2];
    uint8_t count = 0;
    uint8_t exception = 0;
    assert(ModbusRtuCodec::decodeReadResponse(config, response, length, values, count, exception) == RtuDecodeStatus::Ok);
    assert(values[0].kind == ScalarKind::FloatingPoint);
    assert(fabs(values[0].floatingValue - 5.0) < 0.000001);
}

static void testDecodeUpperAndLower16() {
    uint8_t response[9] = {0x01, 0x04, 0x04, 0x00, 0x15, 0x00, 0x20, 0x00, 0x00};
    const size_t length = withCrc(response, 7);

    DecodedScalar values[2];
    uint8_t count = 0;
    uint8_t exception = 0;

    ChannelConfig upper = channel(WireDataType::Input32Upper16);
    assert(ModbusRtuCodec::decodeReadResponse(upper, response, length, values, count, exception) == RtuDecodeStatus::Ok);
    assert(values[0].unsignedValue == 21);

    ChannelConfig lower = channel(WireDataType::Input32Lower16);
    assert(ModbusRtuCodec::decodeReadResponse(lower, response, length, values, count, exception) == RtuDecodeStatus::Ok);
    assert(values[0].unsignedValue == 32);
}

static void testDecodeInt64() {
    uint8_t response[13] = {
        0x01, 0x04, 0x08,
        0x00, 0x15, 0x00, 0x20, 0x00, 0x25, 0x00, 0x30,
        0x00, 0x00,
    };
    const size_t length = withCrc(response, 11);

    ChannelConfig config = channel(WireDataType::Input64ABCDEFGH);
    DecodedScalar values[2];
    uint8_t count = 0;
    uint8_t exception = 0;
    assert(ModbusRtuCodec::decodeReadResponse(config, response, length, values, count, exception) == RtuDecodeStatus::Ok);
    assert(values[0].kind == ScalarKind::UnsignedInteger);
    assert(values[0].unsignedValue == 0x0015002000250030ULL);
}

static void testDecodeCoils() {
    ChannelConfig config = channel(WireDataType::Coil, 2);
    uint8_t response[6] = {0x01, 0x01, 0x01, 0x01, 0x00, 0x00};
    const size_t length = withCrc(response, 4);

    DecodedScalar values[2];
    uint8_t count = 0;
    uint8_t exception = 0;
    assert(ModbusRtuCodec::decodeReadResponse(config, response, length, values, count, exception) == RtuDecodeStatus::Ok);
    assert(count == 2);
    assert(values[0].kind == ScalarKind::Boolean);
    assert(values[0].booleanValue);
    assert(!values[1].booleanValue);
}

static void testExceptionResponse() {
    ChannelConfig config = channel(WireDataType::Hold16AB);
    uint8_t response[5] = {0x01, 0x83, 0x02, 0x00, 0x00};
    const size_t length = withCrc(response, 3);

    DecodedScalar values[2];
    uint8_t count = 0;
    uint8_t exception = 0;
    assert(ModbusRtuCodec::decodeReadResponse(config, response, length, values, count, exception) == RtuDecodeStatus::ExceptionResponse);
    assert(exception == 0x02);
    assert(count == 0);
}

static void testCrcFailure() {
    ChannelConfig config = channel(WireDataType::Hold16AB);
    uint8_t response[7] = {0x01, 0x03, 0x02, 0x00, 0x15, 0x00, 0x00};
    withCrc(response, 5);
    response[3] ^= 0x01;

    DecodedScalar values[2];
    uint8_t count = 0;
    uint8_t exception = 0;
    assert(ModbusRtuCodec::decodeReadResponse(config, response, sizeof(response), values, count, exception) == RtuDecodeStatus::CrcMismatch);
}

int main() {
    testKnownCrcVector();
    testBuildHoldingRegisterRead();
    testBuildTwoInt32ValuesReadsFourRegisters();
    testDecodeTwoSignedInt16Values();
    testDecodeInt32ByteOrders();
    testDecodeFloat();
    testDecodeUpperAndLower16();
    testDecodeInt64();
    testDecodeCoils();
    testExceptionResponse();
    testCrcFailure();
    return 0;
}
