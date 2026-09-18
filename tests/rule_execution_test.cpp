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

int main() {
    testWeeklyTimeCondition();
    testMonthlyTimeCondition();
    testRestartCondition();
    testActions();
    testViennaStyleDstConversion();
    return 0;
}
