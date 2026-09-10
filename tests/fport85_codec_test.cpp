#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lorawan/FPort85Codec.h"

using multibus::lorawan::DecodeStatus;
using multibus::lorawan::EncodeStatus;
using multibus::lorawan::FPort85Codec;
using multibus::lorawan::ModbusChannelCommand;
using multibus::lorawan::ModbusChannelOperation;
using multibus::modbus::WireDataType;

static void testAddChannelReferenceVector() {
    const uint8_t payload[] = {0xff, 0xef, 0x01, 0x01, 0x01, 0xff, 0xff, 0x0a, 0x01};
    ModbusChannelCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeModbusChannelCommand(payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(consumed == sizeof(payload));
    assert(command.operation == ModbusChannelOperation::Upsert);
    assert(command.channel.slot == 0);
    assert(command.channel.slaveId == 1);
    assert(command.channel.address == 65535);
    assert(command.channel.dataType == WireDataType::InputFloatABCD);
    assert(command.channel.quantity == 1);
    assert(!command.channel.signedValue);
}

static void testAddressAndSignedQuantityAreLittleEndian() {
    const uint8_t payload[] = {0xff, 0xef, 0x01, 0x02, 0x07, 0x34, 0x12, 0x0e, 0x11};
    ModbusChannelCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeModbusChannelCommand(payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(command.channel.slot == 1);
    assert(command.channel.slaveId == 7);
    assert(command.channel.address == 0x1234);
    assert(command.channel.dataType == WireDataType::Hold16AB);
    assert(command.channel.quantity == 1);
    assert(command.channel.signedValue);
}

static void testNameReferenceVector() {
    const uint8_t payload[] = {0xff, 0xef, 0x02, 0x06, 0x05, 0x74, 0x65, 0x73, 0x74, 0x36};
    ModbusChannelCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeModbusChannelCommand(payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(command.operation == ModbusChannelOperation::SetName);
    assert(command.channel.slot == 5);
    assert(strcmp(command.channel.name, "test6") == 0);
}

static void testDeleteChannel() {
    const uint8_t payload[] = {0xff, 0xef, 0x00, 0x20};
    ModbusChannelCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeModbusChannelCommand(payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(command.operation == ModbusChannelOperation::Remove);
    assert(command.channel.slot == 31);
}

static void testPeriodicReferenceVector() {
    multibus::modbus::ChannelConfig channel;
    channel.slot = 1;
    channel.slaveId = 1;
    channel.dataType = WireDataType::Input16AB;
    channel.quantity = 1;
    channel.signedValue = true;

    const uint8_t value[] = {0xce, 0xff};
    uint8_t encoded[16] = {0};
    size_t written = 0;
    assert(FPort85Codec::encodePeriodicValue(channel, 0, value, sizeof(value), encoded, sizeof(encoded), written) == EncodeStatus::Ok);
    const uint8_t expected[] = {0xf9, 0x73, 0x01, 0x82, 0xce, 0xff};
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void testFloatUsesSignedUplinkType() {
    multibus::modbus::ChannelConfig channel;
    channel.slot = 0;
    channel.slaveId = 1;
    channel.dataType = WireDataType::InputFloatABCD;
    channel.quantity = 1;
    channel.signedValue = false;

    const uint8_t value[] = {0x00, 0x00, 0xa0, 0x40};
    uint8_t encoded[16] = {0};
    size_t written = 0;
    assert(FPort85Codec::encodePeriodicValue(channel, 0, value, sizeof(value), encoded, sizeof(encoded), written) == EncodeStatus::Ok);
    assert(written == 8);
    assert(encoded[0] == 0xf9 && encoded[1] == 0x73);
    assert(encoded[2] == 0x00);
    assert(encoded[3] == 0x87); // signed + first register + Input_float
}

static void testCollectionException() {
    uint8_t encoded[3] = {0};
    size_t written = 0;
    assert(FPort85Codec::encodeCollectionException(0, encoded, sizeof(encoded), written) == EncodeStatus::Ok);
    const uint8_t expected[] = {0xff, 0x15, 0x00};
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

int main() {
    testAddChannelReferenceVector();
    testAddressAndSignedQuantityAreLittleEndian();
    testNameReferenceVector();
    testDeleteChannel();
    testPeriodicReferenceVector();
    testFloatUsesSignedUplinkType();
    testCollectionException();
    return 0;
}
