#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lorawan/FPort85ReportScheduler.h"
#include "lorawan/FPort85ReportSettings.h"

using multibus::lorawan::FPort85ReportScheduler;
using multibus::lorawan::ReportBuildStatus;
using multibus::lorawan::ReportIntervalCommand;
using multibus::lorawan::ReportIntervalSettings;
using multibus::modbus::ChannelConfig;
using multibus::modbus::DecodedScalar;
using multibus::modbus::ScalarKind;
using multibus::modbus::WireDataType;

static void testReportIntervalReferenceVector() {
    const uint8_t payload[] = {0xff, 0x03, 0xb0, 0x04};
    ReportIntervalCommand command;
    size_t consumed = 0;
    assert(multibus::lorawan::decodeReportIntervalCommand(
        payload, sizeof(payload), command, consumed));
    assert(consumed == sizeof(payload));
    assert(command.settings.seconds == 1200);
}

static void testReportIntervalRange() {
    {
        const uint8_t payload[] = {0xff, 0x03, 0x3b, 0x00}; // 59 s
        ReportIntervalCommand command;
        size_t consumed = 0;
        assert(!multibus::lorawan::decodeReportIntervalCommand(
            payload, sizeof(payload), command, consumed));
        assert(consumed == 0);
    }
    {
        const uint8_t payload[] = {0xff, 0x03, 0x20, 0xfd}; // 64800 s
        ReportIntervalCommand command;
        size_t consumed = 0;
        assert(multibus::lorawan::decodeReportIntervalCommand(
            payload, sizeof(payload), command, consumed));
        assert(command.settings.seconds == 64800);
    }
    {
        const uint8_t payload[] = {0xff, 0x03, 0x21, 0xfd}; // 64801 s
        ReportIntervalCommand command;
        size_t consumed = 0;
        assert(!multibus::lorawan::decodeReportIntervalCommand(
            payload, sizeof(payload), command, consumed));
    }
}

static ChannelConfig input16Channel(uint8_t slot) {
    ChannelConfig channel;
    channel.slot = slot;
    channel.slaveId = 1;
    channel.dataType = WireDataType::Input16AB;
    channel.quantity = 1;
    channel.signedValue = true;
    return channel;
}

static void testReportsLatestSampleAtInterval() {
    FPort85ReportScheduler scheduler;
    ReportIntervalSettings settings;
    settings.seconds = 60;
    scheduler.begin(settings, 0);

    ChannelConfig channel = input16Channel(0);
    DecodedScalar value;
    value.kind = ScalarKind::SignedInteger;
    value.signedValue = -50;
    scheduler.recordPoll(channel, true, &value, 1);

    uint8_t payload[51] = {0};
    size_t written = 0;
    assert(scheduler.preparePacket(59999, payload, sizeof(payload), written) ==
           ReportBuildStatus::NotDue);
    assert(written == 0);

    assert(scheduler.preparePacket(60000, payload, sizeof(payload), written) ==
           ReportBuildStatus::PacketReady);
    const uint8_t expected[] = {0xf9, 0x73, 0x00, 0x82, 0xce, 0xff};
    assert(written == sizeof(expected));
    assert(memcmp(payload, expected, sizeof(expected)) == 0);

    scheduler.markPacketSent(60000);
    assert(!scheduler.reportInProgress());
    assert(scheduler.preparePacket(119999, payload, sizeof(payload), written) ==
           ReportBuildStatus::NotDue);
}

static void testFailureBecomesCollectionException() {
    FPort85ReportScheduler scheduler;
    ReportIntervalSettings settings;
    settings.seconds = 60;
    scheduler.begin(settings, 1000);

    ChannelConfig channel = input16Channel(5);
    scheduler.recordPoll(channel, false, nullptr, 0);

    uint8_t payload[51] = {0};
    size_t written = 0;
    assert(scheduler.preparePacket(61000, payload, sizeof(payload), written) ==
           ReportBuildStatus::PacketReady);
    const uint8_t expected[] = {0xff, 0x15, 0x05};
    assert(written == sizeof(expected));
    assert(memcmp(payload, expected, sizeof(expected)) == 0);
}

static void testQuantityTwoChannelIsNeverSplit() {
    FPort85ReportScheduler scheduler;
    ReportIntervalSettings settings;
    settings.seconds = 60;
    scheduler.begin(settings, 0);

    for (uint8_t slot = 0; slot < 5; ++slot) {
        ChannelConfig channel;
        channel.slot = slot;
        channel.slaveId = 1;
        channel.dataType = WireDataType::Input64ABCDEFGH;
        channel.quantity = 2;
        channel.signedValue = false;

        DecodedScalar values[2];
        values[0].kind = ScalarKind::UnsignedInteger;
        values[0].unsignedValue = static_cast<uint64_t>(slot) + 1U;
        values[1].kind = ScalarKind::UnsignedInteger;
        values[1].unsignedValue = static_cast<uint64_t>(slot) + 101U;
        scheduler.recordPoll(channel, true, values, 2);
    }

    uint8_t payload[51] = {0};
    size_t written = 0;

    assert(scheduler.preparePacket(60000, payload, sizeof(payload), written) ==
           ReportBuildStatus::PacketReady);
    assert(written == 48); // two complete 24-byte channels
    scheduler.markPacketSent(60000);

    assert(scheduler.preparePacket(60001, payload, sizeof(payload), written) ==
           ReportBuildStatus::PacketReady);
    assert(written == 48);
    scheduler.markPacketSent(60001);

    assert(scheduler.preparePacket(60002, payload, sizeof(payload), written) ==
           ReportBuildStatus::PacketReady);
    assert(written == 24);
    scheduler.markPacketSent(60002);
    assert(!scheduler.reportInProgress());
}

static void testFailedSendRetainsIdenticalPacketForRetry() {
    FPort85ReportScheduler scheduler;
    ReportIntervalSettings settings;
    settings.seconds = 60;
    scheduler.begin(settings, 0);

    ChannelConfig channel = input16Channel(0);
    DecodedScalar value;
    value.kind = ScalarKind::SignedInteger;
    value.signedValue = 42;
    scheduler.recordPoll(channel, true, &value, 1);

    uint8_t first[51] = {0};
    size_t firstLength = 0;
    assert(scheduler.preparePacket(60000, first, sizeof(first), firstLength) ==
           ReportBuildStatus::PacketReady);
    scheduler.markPacketFailed(60000);

    uint8_t retry[51] = {0};
    size_t retryLength = 0;
    assert(scheduler.preparePacket(64999, retry, sizeof(retry), retryLength) ==
           ReportBuildStatus::NotDue);
    assert(scheduler.preparePacket(65000, retry, sizeof(retry), retryLength) ==
           ReportBuildStatus::PacketReady);
    assert(firstLength == retryLength);
    assert(memcmp(first, retry, firstLength) == 0);
}

static void testImmediateReportDoesNotShiftPeriodicSchedule() {
    FPort85ReportScheduler scheduler;
    ReportIntervalSettings settings;
    settings.seconds = 60;
    scheduler.begin(settings, 0);

    ChannelConfig channel = input16Channel(0);
    DecodedScalar value;
    value.kind = ScalarKind::SignedInteger;
    value.signedValue = 7;
    scheduler.recordPoll(channel, true, &value, 1);

    scheduler.requestImmediateReport();

    uint8_t payload[51] = {0};
    size_t written = 0;
    assert(scheduler.preparePacket(1000, payload, sizeof(payload), written) ==
           ReportBuildStatus::PacketReady);
    scheduler.markPacketSent(1000);
    assert(!scheduler.reportInProgress());
    assert(scheduler.nextReportAtMs() == 60000);

    assert(scheduler.preparePacket(59999, payload, sizeof(payload), written) ==
           ReportBuildStatus::NotDue);
    assert(scheduler.preparePacket(60000, payload, sizeof(payload), written) ==
           ReportBuildStatus::PacketReady);
}

static void testImmediateRequestQueuesBehindActiveReport() {
    FPort85ReportScheduler scheduler;
    ReportIntervalSettings settings;
    settings.seconds = 60;
    scheduler.begin(settings, 0);

    for (uint8_t slot = 0; slot < 3; ++slot) {
        ChannelConfig channel;
        channel.slot = slot;
        channel.slaveId = 1;
        channel.dataType = WireDataType::Input64ABCDEFGH;
        channel.quantity = 2;

        DecodedScalar values[2];
        values[0].kind = ScalarKind::UnsignedInteger;
        values[0].unsignedValue = slot + 1U;
        values[1].kind = ScalarKind::UnsignedInteger;
        values[1].unsignedValue = slot + 11U;
        scheduler.recordPoll(channel, true, values, 2);
    }

    uint8_t payload[51] = {0};
    size_t written = 0;
    assert(scheduler.preparePacket(60000, payload, sizeof(payload), written) ==
           ReportBuildStatus::PacketReady);
    scheduler.requestImmediateReport();

    scheduler.markPacketSent(60000);
    assert(scheduler.preparePacket(60001, payload, sizeof(payload), written) ==
           ReportBuildStatus::PacketReady);
    scheduler.markPacketSent(60001);
    assert(!scheduler.reportInProgress());

    assert(scheduler.preparePacket(60002, payload, sizeof(payload), written) ==
           ReportBuildStatus::PacketReady);
}

int main() {
    testReportIntervalReferenceVector();
    testReportIntervalRange();
    testReportsLatestSampleAtInterval();
    testFailureBecomesCollectionException();
    testQuantityTwoChannelIsNeverSplit();
    testFailedSendRetainsIdenticalPacketForRetry();
    testImmediateReportDoesNotShiftPeriodicSchedule();
    testImmediateRequestQueuesBehindActiveReport();
    return 0;
}
