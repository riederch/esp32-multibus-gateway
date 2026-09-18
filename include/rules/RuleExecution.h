#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rules/RuleState.h"
#include "time/LocalTime.h"

namespace multibus::rules {

enum class ExecutableAction : uint8_t {
    None,
    UploadData,
    RawRs485,
    Reboot,
    Unsupported,
};

struct ActionPlan {
    ExecutableAction action = ExecutableAction::None;
    uint32_t delayMs = 0;
    const uint8_t* payload = nullptr;
    uint8_t payloadLength = 0;
};

inline bool matchesTimeCondition(const StoredFrame& frame,
                                 const time::LocalDateTime& local) {
    if (!frame.present() || frame.length != 11 ||
        frame.data[0] != 0xf9 || frame.data[1] != 0x7d ||
        frame.data[3] != 0x11) {
        return false;
    }

    const uint8_t mode = frame.data[4];
    if (mode > 1 || frame.data[9] > 23 || frame.data[10] > 59) return false;
    if (local.hour != frame.data[9] || local.minute != frame.data[10]) return false;

    const uint32_t mask =
        static_cast<uint32_t>(frame.data[5]) |
        (static_cast<uint32_t>(frame.data[6]) << 8U) |
        (static_cast<uint32_t>(frame.data[7]) << 16U) |
        (static_cast<uint32_t>(frame.data[8]) << 24U);

    if (mode == 0) {
        if (local.weekday < 1 || local.weekday > 7) return false;
        return (mask & (1UL << (local.weekday - 1U))) != 0;
    }

    if (local.day < 1 || local.day > 31) return false;
    return (mask & (1UL << (local.day - 1U))) != 0;
}

inline bool isDeviceRestartCondition(const StoredFrame& frame) {
    return frame.present() &&
           frame.length == 4 &&
           frame.data[0] == 0xf9 &&
           frame.data[1] == 0x7d &&
           frame.data[3] == 0x16;
}

inline ActionPlan decodeExecutableAction(const StoredFrame& frame) {
    ActionPlan plan;
    if (!frame.present() || frame.length < 4 ||
        frame.data[0] != 0xf9 || frame.data[1] != 0x7d) {
        return plan;
    }

    const uint8_t actionKind = frame.data[3] & 0x0fU;
    if (actionKind == 0x00U) return plan;

    if (frame.length < 8) {
        plan.action = ExecutableAction::Unsupported;
        return plan;
    }

    plan.delayMs =
        static_cast<uint32_t>(frame.data[4]) |
        (static_cast<uint32_t>(frame.data[5]) << 8U) |
        (static_cast<uint32_t>(frame.data[6]) << 16U) |
        (static_cast<uint32_t>(frame.data[7]) << 24U);

    if (plan.delayMs > 86400000UL) {
        plan.action = ExecutableAction::Unsupported;
        return plan;
    }

    if (actionKind == 0x03U && frame.length >= 11) {
        const uint8_t messageLength = frame.data[8];
        if (messageLength < 2 || messageLength > 48 ||
            frame.length != static_cast<size_t>(9U + messageLength)) {
            plan.action = ExecutableAction::Unsupported;
            return plan;
        }
        plan.action = ExecutableAction::RawRs485;
        plan.payload = frame.data + 9;
        plan.payloadLength = messageLength;
    } else if (actionKind == 0x04U && frame.length == 8) {
        plan.action = ExecutableAction::UploadData;
    } else if (actionKind == 0x06U && frame.length == 8) {
        plan.action = ExecutableAction::Reboot;
    } else {
        plan.action = ExecutableAction::Unsupported;
    }
    return plan;
}

inline uint32_t localMinuteKey(const time::LocalDateTime& local) {
    // Compact key sufficient to suppress duplicate evaluation within one minute.
    return static_cast<uint32_t>((local.year - 2000) & 0x7f) << 25U |
           static_cast<uint32_t>(local.month & 0x0fU) << 21U |
           static_cast<uint32_t>(local.day & 0x1fU) << 16U |
           static_cast<uint32_t>(local.hour & 0x1fU) << 11U |
           static_cast<uint32_t>(local.minute & 0x3fU) << 5U;
}

} // namespace multibus::rules
