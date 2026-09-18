#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lorawan/FPort85Codec.h"

using multibus::lorawan::BasicControlCommand;
using multibus::lorawan::DecodeStatus;
using multibus::lorawan::EncodeStatus;
using multibus::lorawan::FPort85Codec;
using multibus::lorawan::ModbusChannelCommand;
using multibus::lorawan::ModbusChannelOperation;
using multibus::lorawan::ModbusMasterSettingsCommand;
using multibus::lorawan::PeriodicReportEnquiryCommand;
using multibus::lorawan::Rs485SettingsCommand;
using multibus::lorawan::Rs485SettingsEnquiryCommand;
using multibus::lorawan::Rs485SettingsEnquiryKind;
using multibus::modbus::PassThroughMode;
using multibus::modbus::Rs485Parity;
using multibus::modbus::Rs485StopBits;
using multibus::modbus::WireDataType;

static void testRs485SettingsReferenceVector() {
    const uint8_t payload[] = {0xf9, 0x78, 0x80, 0x25, 0x00, 0x00, 0x08, 0x01, 0x00};
    Rs485SettingsCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeRs485SettingsCommand(payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(consumed == sizeof(payload));
    assert(command.settings.baudRate == 9600);
    assert(command.settings.dataBits == 8);
    assert(command.settings.stopBits == Rs485StopBits::One);
    assert(command.settings.parity == Rs485Parity::None);
}

static void testRs485SettingsAcceptsProtocolValues() {
    const uint8_t payload[] = {0xf9, 0x78, 0x00, 0xc2, 0x01, 0x00, 0x09, 0x03, 0x02};
    Rs485SettingsCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeRs485SettingsCommand(payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(command.settings.baudRate == 115200);
    assert(command.settings.dataBits == 9);
    assert(command.settings.stopBits == Rs485StopBits::OnePointFive);
    assert(command.settings.parity == Rs485Parity::Odd);
    assert(!multibus::modbus::esp32SupportsRs485SerialSettings(command.settings));
}

static void testRs485SettingsRejectsUnknownBaud() {
    const uint8_t payload[] = {0xf9, 0x78, 0x10, 0x27, 0x00, 0x00, 0x08, 0x01, 0x00};
    Rs485SettingsCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeRs485SettingsCommand(payload, sizeof(payload), command, consumed) == DecodeStatus::Invalid);
    assert(consumed == 0);
}

static void testModbusMasterSettingsReferenceVector() {
    const uint8_t payload[] = {0xf9, 0x79, 0x32, 0x00, 0x60, 0xea, 0x03, 0x10, 0x05};
    ModbusMasterSettingsCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeModbusMasterSettingsCommand(payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(consumed == sizeof(payload));
    assert(command.settings.executionIntervalMs == 50);
    assert(command.settings.maxResponseTimeMs == 60000);
    assert(command.settings.maxRetryTimes == 3);
    assert(command.settings.passThroughMode == PassThroughMode::Active);
    assert(command.settings.passThroughPort == 5);
    assert(!multibus::modbus::runtimeSupportsModbusMasterSettings(command.settings));
}

static void testModbusMasterSettingsDefaultsAreRuntimeSupported() {
    const uint8_t payload[] = {0xf9, 0x79, 0x32, 0x00, 0xf4, 0x01, 0x03, 0x00, 0x02};
    ModbusMasterSettingsCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeModbusMasterSettingsCommand(payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(command.settings.executionIntervalMs == 50);
    assert(command.settings.maxResponseTimeMs == 500);
    assert(command.settings.maxRetryTimes == 3);
    assert(command.settings.passThroughMode == PassThroughMode::Disabled);
    assert(command.settings.passThroughPort == 2);
    assert(multibus::modbus::runtimeSupportsModbusMasterSettings(command.settings));
}

static void testModbusMasterSettingsRejectsInvalidInterval() {
    const uint8_t payload[] = {0xf9, 0x79, 0x09, 0x00, 0xf4, 0x01, 0x03, 0x00, 0x02};
    ModbusMasterSettingsCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeModbusMasterSettingsCommand(payload, sizeof(payload), command, consumed) == DecodeStatus::Invalid);
    assert(consumed == 0);
}

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
    assert(encoded[3] == 0x87);
}

static void testCollectionException() {
    uint8_t encoded[3] = {0};
    size_t written = 0;
    assert(FPort85Codec::encodeCollectionException(0, encoded, sizeof(encoded), written) == EncodeStatus::Ok);
    const uint8_t expected[] = {0xff, 0x15, 0x00};
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void testRs485SettingsEnquirySerialReferenceVector() {
    const uint8_t payload[] = {0xf9, 0x7a, 0x00};
    Rs485SettingsEnquiryCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeRs485SettingsEnquiryCommand(
        payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(consumed == sizeof(payload));
    assert(command.kind == Rs485SettingsEnquiryKind::Serial);

    multibus::modbus::Rs485SerialSettings serial;
    serial.baudRate = 9600;
    serial.dataBits = 8;
    serial.stopBits = Rs485StopBits::One;
    serial.parity = Rs485Parity::None;
    multibus::modbus::ModbusMasterSettings master;

    uint8_t reply[16] = {0};
    size_t written = 0;
    assert(FPort85Codec::encodeRs485SettingsEnquiryReply(
        command, serial, master, reply, sizeof(reply), written) == EncodeStatus::Ok);
    const uint8_t expected[] = {
        0xf8, 0x7a, 0x00, 0x00,
        0xf9, 0x78, 0x80, 0x25, 0x00, 0x00, 0x08, 0x01, 0x00
    };
    assert(written == sizeof(expected));
    assert(memcmp(reply, expected, sizeof(expected)) == 0);
}

static void testRs485SettingsEnquiryModbusReply() {
    const uint8_t payload[] = {0xf9, 0x7a, 0x01};
    Rs485SettingsEnquiryCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeRs485SettingsEnquiryCommand(
        payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(command.kind == Rs485SettingsEnquiryKind::Modbus);

    multibus::modbus::Rs485SerialSettings serial;
    multibus::modbus::ModbusMasterSettings master;
    master.executionIntervalMs = 50;
    master.maxResponseTimeMs = 500;
    master.maxRetryTimes = 3;
    master.passThroughMode = PassThroughMode::Disabled;
    master.passThroughPort = 2;

    uint8_t reply[16] = {0};
    size_t written = 0;
    assert(FPort85Codec::encodeRs485SettingsEnquiryReply(
        command, serial, master, reply, sizeof(reply), written) == EncodeStatus::Ok);
    const uint8_t expected[] = {
        0xf8, 0x7a, 0x01, 0x00,
        0xf9, 0x79, 0x32, 0x00, 0xf4, 0x01, 0x03, 0x00, 0x02
    };
    assert(written == sizeof(expected));
    assert(memcmp(reply, expected, sizeof(expected)) == 0);
}

static void testRs485SettingsEnquiryRejectsUnknownKind() {
    const uint8_t payload[] = {0xf9, 0x7a, 0x02};
    Rs485SettingsEnquiryCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodeRs485SettingsEnquiryCommand(
        payload, sizeof(payload), command, consumed) == DecodeStatus::Invalid);
    assert(consumed == 0);
}

static void testRebootReferenceVector() {
    const uint8_t payload[] = {0xff, 0x10, 0xff};
    BasicControlCommand command = BasicControlCommand::Rejoin;
    size_t consumed = 0;
    assert(FPort85Codec::decodeBasicControlCommand(
        payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(consumed == sizeof(payload));
    assert(command == BasicControlCommand::Reboot);
}

static void testRejoinReferenceVector() {
    const uint8_t payload[] = {0xff, 0x04, 0xff};
    BasicControlCommand command = BasicControlCommand::Reboot;
    size_t consumed = 0;
    assert(FPort85Codec::decodeBasicControlCommand(
        payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(consumed == sizeof(payload));
    assert(command == BasicControlCommand::Rejoin);
}

static void testBasicControlRejectsInvalidMagic() {
    const uint8_t payload[] = {0xff, 0x10, 0x00};
    BasicControlCommand command = BasicControlCommand::Rejoin;
    size_t consumed = 0;
    assert(FPort85Codec::decodeBasicControlCommand(
        payload, sizeof(payload), command, consumed) == DecodeStatus::Invalid);
    assert(consumed == 0);
}

static void testPeriodicReportEnquiryReferenceVector() {
    const uint8_t payload[] = {0xff, 0x28, 0xff};
    PeriodicReportEnquiryCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodePeriodicReportEnquiryCommand(
        payload, sizeof(payload), command, consumed) == DecodeStatus::Ok);
    assert(consumed == sizeof(payload));
}

static void testPeriodicReportEnquiryRejectsInvalidMagic() {
    const uint8_t payload[] = {0xff, 0x28, 0x00};
    PeriodicReportEnquiryCommand command;
    size_t consumed = 0;
    assert(FPort85Codec::decodePeriodicReportEnquiryCommand(
        payload, sizeof(payload), command, consumed) == DecodeStatus::Invalid);
    assert(consumed == 0);
}

int main() {
    testPeriodicReportEnquiryReferenceVector();
    testPeriodicReportEnquiryRejectsInvalidMagic();
    testRebootReferenceVector();
    testRejoinReferenceVector();
    testBasicControlRejectsInvalidMagic();
    testRs485SettingsReferenceVector();
    testRs485SettingsAcceptsProtocolValues();
    testRs485SettingsRejectsUnknownBaud();
    testModbusMasterSettingsReferenceVector();
    testModbusMasterSettingsDefaultsAreRuntimeSupported();
    testModbusMasterSettingsRejectsInvalidInterval();
    testRs485SettingsEnquirySerialReferenceVector();
    testRs485SettingsEnquiryModbusReply();
    testRs485SettingsEnquiryRejectsUnknownKind();
    testAddChannelReferenceVector();
    testAddressAndSignedQuantityAreLittleEndian();
    testNameReferenceVector();
    testDeleteChannel();
    testPeriodicReferenceVector();
    testFloatUsesSignedUplinkType();
    testCollectionException();
    return 0;
}
