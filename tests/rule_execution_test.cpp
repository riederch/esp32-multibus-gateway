#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "rules/RuleExecution.h"

using multibus::rules::ActionPlan;
using multibus::rules::ExecutableAction;
using multibus::rules::StoredFrame;
using multibus::rules::decodeExecutableAction;
using multibus::rules::isDeviceRestartCondition;
using multibus::rules::matchesTimeCondition;
using multibus::rules::matchesServerMessageCondition;
using multibus::rules::ChannelConditionPlan;
using multibus::rules::ChannelConditionRuntime;
using multibus::rules::decodeChannelCondition;
using multibus::rules::evaluateChannelCondition;
using multibus::rules::validServerMessage;
using multibus::time::DstSettings;
using multibus::time::LocalDateTime;
using multibus::time::Settings;
using multibus::time::localDateTime;

static StoredFrame frame(const uint8_t* data, size_t length) {
    StoredFrame result;
    assert(result.set(data, length));
    return result;
}

static void testWeeklyTimeCondition() {
    const uint8_t data[] = {
        0xf9, 0x7d, 0x81, 0x11,
        0x00, 0x01, 0x00, 0x00, 0x00,
        0x08, 0x05
    };
    const StoredFrame condition = frame(data, sizeof(data));

    LocalDateTime monday;
    monday.weekday = 1;
    monday.hour = 8;
    monday.minute = 5;
    assert(matchesTimeCondition(condition, monday));

    monday.minute = 6;
    assert(!matchesTimeCondition(condition, monday));

    LocalDateTime tuesday = monday;
    tuesday.weekday = 2;
    tuesday.minute = 5;
    assert(!matchesTimeCondition(condition, tuesday));
}

static void testMonthlyTimeCondition() {
    const uint8_t data[] = {
        0xf9, 0x7d, 0x81, 0x11,
        0x01, 0x00, 0x02, 0x00, 0x00,
        0x12, 0x1e
    };
    const StoredFrame condition = frame(data, sizeof(data));

    LocalDateTime local;
    local.day = 10;
    local.hour = 18;
    local.minute = 30;
    assert(matchesTimeCondition(condition, local));
    local.day = 9;
    assert(!matchesTimeCondition(condition, local));
}

static void testRestartCondition() {
    const uint8_t data[] = {0xf9, 0x7d, 0x81, 0x16};
    const StoredFrame condition = frame(data, sizeof(data));
    assert(isDeviceRestartCondition(condition));
}

static void testActions() {
    const uint8_t upload[] = {0xf9, 0x7d, 0x81, 0x94, 0xe8, 0x03, 0x00, 0x00};
    const ActionPlan uploadPlan = decodeExecutableAction(frame(upload, sizeof(upload)));
    assert(uploadPlan.action == ExecutableAction::UploadData);
    assert(uploadPlan.delayMs == 1000);

    const uint8_t message[] = {
        0xf9, 0x7d, 0x81, 0x91,
        0xfa, 0x00, 0x00, 0x00,
        0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f
    };
    const ActionPlan messagePlan = decodeExecutableAction(frame(message, sizeof(message)));
    assert(messagePlan.action == ExecutableAction::ServerMessage);
    assert(messagePlan.delayMs == 250);
    assert(messagePlan.payloadLength == 5);
    assert(memcmp(messagePlan.payload, "hello", 5) == 0);

    const uint8_t raw[] = {
        0xf9, 0x7d, 0x81, 0x93,
        0xf4, 0x01, 0x00, 0x00,
        0x02, 0x01, 0x03
    };
    const ActionPlan rawPlan = decodeExecutableAction(frame(raw, sizeof(raw)));
    assert(rawPlan.action == ExecutableAction::RawRs485);
    assert(rawPlan.delayMs == 500);
    assert(rawPlan.payloadLength == 2);
    assert(rawPlan.payload[0] == 0x01 && rawPlan.payload[1] == 0x03);

    const uint8_t reboot[] = {0xf9, 0x7d, 0x81, 0xa6, 0x00, 0x00, 0x00, 0x00};
    const ActionPlan rebootPlan = decodeExecutableAction(frame(reboot, sizeof(reboot)));
    assert(rebootPlan.action == ExecutableAction::Reboot);
}

static void testViennaStyleDstConversion() {
    Settings settings;
    settings.utcOffsetMinutes = 60;
    settings.dst.enabled = true;
    settings.dst.biasMinutes = 60;
    settings.dst.start.month = 3;
    settings.dst.start.week = 5;
    settings.dst.start.weekday = 7;
    settings.dst.start.minuteOfDay = 60;
    settings.dst.end.month = 10;
    settings.dst.end.week = 5;
    settings.dst.end.weekday = 7;
    settings.dst.end.minuteOfDay = 60;

    // 2026-07-01 12:00:00 UTC -> 14:00 local with DST.
    const LocalDateTime summer = localDateTime(1782907200U, settings);
    assert(summer.year == 2026);
    assert(summer.month == 7);
    assert(summer.day == 1);
    assert(summer.hour == 14);

    // 2026-01-01 12:00:00 UTC -> 13:00 local standard time.
    const LocalDateTime winter = localDateTime(1767268800U, settings);
    assert(winter.year == 2026);
    assert(winter.month == 1);
    assert(winter.day == 1);
    assert(winter.hour == 13);
}

static void testServerMessageCondition() {
    const uint8_t conditionData[] = {
        0xf9, 0x7d, 0x81, 0x14,
        0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f
    };
    const StoredFrame condition = frame(conditionData, sizeof(conditionData));
    const uint8_t hello[] = {'h','e','l','l','o'};
    const uint8_t other[] = {'h','e','l','l','!'};
    assert(validServerMessage(hello, sizeof(hello)));
    assert(matchesServerMessageCondition(condition, hello, sizeof(hello)));
    assert(!matchesServerMessageCondition(condition, other, sizeof(other)));

    const uint8_t binary[] = {0xff, 0x10, 0xff};
    assert(!validServerMessage(binary, sizeof(binary)));
}

static void testDocumentedAboveChannelCondition() {
    const uint8_t data[] = {
        0xf9, 0x7d, 0x82, 0x12,
        0x04, 0x13,
        0x10, 0x27, 0x00, 0x00,
        0x88, 0x13, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xa0, 0x40
    };
    ChannelConditionPlan plan;
    assert(decodeChannelCondition(frame(data, sizeof(data)), plan));
    assert(plan.channelId == 4);
    assert(plan.continueMode == 1);
    assert(plan.continueTimeMs == 10000);
    assert(plan.lockTimeMs == 5000);
    assert(plan.maximum == 5.0f);

    multibus::modbus::DecodedScalar value;
    value.kind = multibus::modbus::ScalarKind::FloatingPoint;
    value.floatingValue = 6.0;

    ChannelConditionRuntime runtime;
    assert(!evaluateChannelCondition(plan, value, 1000, runtime));
    assert(!evaluateChannelCondition(plan, value, 10999, runtime));
    assert(evaluateChannelCondition(plan, value, 11000, runtime));
    assert(!evaluateChannelCondition(plan, value, 12000, runtime));

    value.floatingValue = 4.0;
    assert(!evaluateChannelCondition(plan, value, 16000, runtime));
    value.floatingValue = 7.0;
    assert(!evaluateChannelCondition(plan, value, 17000, runtime));
    assert(evaluateChannelCondition(plan, value, 27000, runtime));
}

static void testBooleanImmediateCondition() {
    const uint8_t data[] = {
        0xf9, 0x7d, 0x81, 0x12,
        0x01, 0x01,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    ChannelConditionPlan plan;
    assert(decodeChannelCondition(frame(data, sizeof(data)), plan));

    multibus::modbus::DecodedScalar value;
    value.kind = multibus::modbus::ScalarKind::Boolean;
    value.booleanValue = true;
    ChannelConditionRuntime runtime;
    assert(evaluateChannelCondition(plan, value, 1, runtime));
    assert(!evaluateChannelCondition(plan, value, 2, runtime));
    value.booleanValue = false;
    assert(!evaluateChannelCondition(plan, value, 3, runtime));
    value.booleanValue = true;
    assert(evaluateChannelCondition(plan, value, 4, runtime));
}

static void testChangeRecentCondition() {
    const uint8_t data[] = {
        0xf9, 0x7d, 0x81, 0x12,
        0x01, 0x06,
        0,0,0,0, 0,0,0,0,
        0,0,0,0,
        0x00,0x00,0x20,0x40
    };
    ChannelConditionPlan plan;
    assert(decodeChannelCondition(frame(data, sizeof(data)), plan));

    multibus::modbus::DecodedScalar value;
    value.kind = multibus::modbus::ScalarKind::FloatingPoint;
    value.floatingValue = 10.0;
    ChannelConditionRuntime runtime;
    assert(!evaluateChannelCondition(plan, value, 100, runtime));
    value.floatingValue = 11.0;
    assert(!evaluateChannelCondition(plan, value, 200, runtime));
    value.floatingValue = 13.5;
    assert(evaluateChannelCondition(plan, value, 300, runtime));
}

int main() {
    testDocumentedAboveChannelCondition();
    testBooleanImmediateCondition();
    testChangeRecentCondition();
    testServerMessageCondition();
    testWeeklyTimeCondition();
    testMonthlyTimeCondition();
    testRestartCondition();
    testActions();
    testViennaStyleDstConversion();
    return 0;
}
