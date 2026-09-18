#pragma once

#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include "modbus/ModbusRtuCodec.h"
#include "rules/RuleState.h"
#include "time/LocalTime.h"

namespace multibus::rules {

enum class ExecutableAction : uint8_t {
    None,
    ServerMessage,
    UploadData,
    RawRs485,
    UploadAlarm,
    Reboot,
    Unsupported,
};

struct ActionPlan {
    ExecutableAction action = ExecutableAction::None;
    uint32_t delayMs = 0;
    const uint8_t* payload = nullptr;
    uint8_t payloadLength = 0;
    bool thresholdReleaseEnabled = false;
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

inline bool allowedServerMessageByte(uint8_t value) {
    return (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') ||
           value == ',' || value == '.' || value == ' ' || value == '!';
}

inline bool validServerMessage(const uint8_t* payload, size_t length) {
    if (payload == nullptr || length < 2 || length > 48) return false;
    for (size_t i = 0; i < length; ++i) {
        if (!allowedServerMessageByte(payload[i])) return false;
    }
    return true;
}

inline bool matchesServerMessageCondition(const StoredFrame& frame,
                                          const uint8_t* payload,
                                          size_t length) {
    if (!validServerMessage(payload, length) ||
        !frame.present() || frame.length < 7 ||
        frame.data[0] != 0xf9 || frame.data[1] != 0x7d ||
        frame.data[3] != 0x14) {
        return false;
    }

    const uint8_t expectedLength = frame.data[4];
    return expectedLength == length &&
           frame.length == static_cast<size_t>(5U + expectedLength) &&
           memcmp(frame.data + 5, payload, length) == 0;
}

inline bool matchesRs485CommandCondition(const StoredFrame& frame,
                                               const uint8_t* payload,
                                               size_t length) {
    if (payload == nullptr || length < 2 || length > 48 ||
        !frame.present() || frame.length < 7 ||
        frame.data[0] != 0xf9 || frame.data[1] != 0x7d ||
        frame.data[3] != 0x13) {
        return false;
    }

    const uint8_t expectedLength = frame.data[4];
    return expectedLength == length &&
           frame.length == static_cast<size_t>(5U + expectedLength) &&
           memcmp(frame.data + 5, payload, length) == 0;
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

    if (actionKind == 0x01U && frame.length >= 10) {
        const uint8_t messageLength = frame.data[8];
        if (messageLength < 1 || messageLength > 48 ||
            frame.length != static_cast<size_t>(9U + messageLength)) {
            plan.action = ExecutableAction::Unsupported;
            return plan;
        }
        plan.action = ExecutableAction::ServerMessage;
        plan.payload = frame.data + 9;
        plan.payloadLength = messageLength;
    } else if (actionKind == 0x03U && frame.length >= 11) {
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
    } else if (actionKind == 0x05U && frame.length == 9) {
        if (frame.data[8] > 0x01U) {
            plan.action = ExecutableAction::Unsupported;
            return plan;
        }
        plan.action = ExecutableAction::UploadAlarm;
        plan.thresholdReleaseEnabled = frame.data[8] == 0x01U;
    } else if (actionKind == 0x06U && frame.length == 8) {
        plan.action = ExecutableAction::Reboot;
    } else {
        plan.action = ExecutableAction::Unsupported;
    }
    return plan;
}

enum class ChannelThresholdMode : uint8_t {
    FalseValue = 0,
    TrueValue = 1,
    Below = 2,
    Above = 3,
    Within = 4,
    ChangeRecent = 6,
    ChangeInterval = 7,
};

struct ChannelConditionPlan {
    uint8_t channelId = 0;
    uint8_t continueMode = 0;
    ChannelThresholdMode mode = ChannelThresholdMode::FalseValue;
    uint32_t continueTimeMs = 0;
    uint32_t lockTimeMs = 0;
    float minimum = 0.0f;
    float maximum = 0.0f;
    uint32_t changeIntervalMs = 0;
};

struct ChannelConditionRuntime {
    bool active = false;
    bool firedForActive = false;
    uint32_t activeSinceMs = 0;
    uint32_t lockedUntilMs = 0;
    bool havePrevious = false;
    double previousValue = 0.0;
    uint32_t previousAtMs = 0;
};

inline uint32_t readU32Le(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8U) |
           (static_cast<uint32_t>(data[2]) << 16U) |
           (static_cast<uint32_t>(data[3]) << 24U);
}

inline float readFloat32Le(const uint8_t* data) {
    const uint32_t bits = readU32Le(data);
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

inline bool decodeChannelCondition(const StoredFrame& frame,
                                   ChannelConditionPlan& plan) {
    if (!frame.present() || frame.length != 22 ||
        frame.data[0] != 0xf9 || frame.data[1] != 0x7d ||
        frame.data[3] != 0x12) {
        return false;
    }

    const uint8_t channelId = frame.data[4];
    const uint8_t modeByte = frame.data[5];
    const uint8_t thresholdMode = modeByte & 0x0fU;
    const uint8_t continueMode = (modeByte >> 4U) & 0x0fU;
    if (channelId < 1 || channelId > 32 ||
        continueMode > 1 ||
        !((thresholdMode <= 4) || thresholdMode == 6 || thresholdMode == 7)) {
        return false;
    }

    plan = ChannelConditionPlan{};
    plan.channelId = channelId;
    plan.continueMode = continueMode;
    plan.mode = static_cast<ChannelThresholdMode>(thresholdMode);
    plan.continueTimeMs = readU32Le(frame.data + 6);
    plan.lockTimeMs = readU32Le(frame.data + 10);
    if (plan.mode == ChannelThresholdMode::ChangeInterval) {
        plan.changeIntervalMs = readU32Le(frame.data + 14);
        plan.maximum = readFloat32Le(frame.data + 18);
        plan.minimum = 0.0f;
    } else {
        plan.minimum = readFloat32Le(frame.data + 14);
        plan.maximum = readFloat32Le(frame.data + 18);
    }

    if (plan.continueTimeMs > 86400000UL || plan.lockTimeMs > 86400000UL) {
        return false;
    }
    if (plan.changeIntervalMs > 86400000UL ||
        !isfinite(plan.minimum) || !isfinite(plan.maximum)) return false;
    return true;
}

inline bool scalarToDouble(const modbus::DecodedScalar& scalar, double& value) {
    switch (scalar.kind) {
        case modbus::ScalarKind::Boolean:
            value = scalar.booleanValue ? 1.0 : 0.0;
            return true;
        case modbus::ScalarKind::SignedInteger:
            value = static_cast<double>(scalar.signedValue);
            return true;
        case modbus::ScalarKind::UnsignedInteger:
            value = static_cast<double>(scalar.unsignedValue);
            return true;
        case modbus::ScalarKind::FloatingPoint:
            if (!isfinite(scalar.floatingValue)) return false;
            value = scalar.floatingValue;
            return true;
    }
    return false;
}

inline bool thresholdPredicate(const ChannelConditionPlan& plan,
                               const modbus::DecodedScalar& scalar) {
    double value = 0.0;
    if (!scalarToDouble(scalar, value)) return false;

    switch (plan.mode) {
        case ChannelThresholdMode::FalseValue:
            return scalar.kind == modbus::ScalarKind::Boolean && !scalar.booleanValue;
        case ChannelThresholdMode::TrueValue:
            return scalar.kind == modbus::ScalarKind::Boolean && scalar.booleanValue;
        case ChannelThresholdMode::Below:
            return value < static_cast<double>(plan.minimum);
        case ChannelThresholdMode::Above:
            return value > static_cast<double>(plan.maximum);
        case ChannelThresholdMode::Within:
            return value >= static_cast<double>(plan.minimum) &&
                   value <= static_cast<double>(plan.maximum);
        case ChannelThresholdMode::ChangeRecent:
        case ChannelThresholdMode::ChangeInterval:
            return false;
    }
    return false;
}

inline bool evaluateChannelCondition(const ChannelConditionPlan& plan,
                                     const modbus::DecodedScalar& scalar,
                                     uint32_t nowMs,
                                     ChannelConditionRuntime& runtime) {
    double value = 0.0;
    if (!scalarToDouble(scalar, value)) return false;

    if (plan.mode == ChannelThresholdMode::ChangeRecent) {
        bool fire = false;
        if (runtime.havePrevious &&
            static_cast<int32_t>(nowMs - runtime.lockedUntilMs) >= 0) {
            fire = fabs(value - runtime.previousValue) >=
                   static_cast<double>(plan.maximum);
        }
        runtime.previousValue = value;
        runtime.previousAtMs = nowMs;
        runtime.havePrevious = true;
        if (fire) runtime.lockedUntilMs = nowMs + plan.lockTimeMs;
        return fire;
    }

    if (plan.mode == ChannelThresholdMode::ChangeInterval) {
        if (!runtime.havePrevious) {
            runtime.previousValue = value;
            runtime.previousAtMs = nowMs;
            runtime.havePrevious = true;
            return false;
        }

        if (static_cast<uint32_t>(nowMs - runtime.previousAtMs) <
            plan.changeIntervalMs) {
            return false;
        }

        const bool fire =
            static_cast<int32_t>(nowMs - runtime.lockedUntilMs) >= 0 &&
            fabs(value - runtime.previousValue) >=
                static_cast<double>(plan.maximum);
        runtime.previousValue = value;
        runtime.previousAtMs = nowMs;
        if (fire) runtime.lockedUntilMs = nowMs + plan.lockTimeMs;
        return fire;
    }

    const bool predicate = thresholdPredicate(plan, scalar);
    if (static_cast<int32_t>(nowMs - runtime.lockedUntilMs) < 0) {
        if (!predicate) {
            runtime.active = false;
            runtime.firedForActive = false;
        }
        return false;
    }

    if (!predicate) {
        if (runtime.active && !runtime.firedForActive &&
            plan.continueMode == 0 && plan.continueTimeMs > 0 &&
            static_cast<uint32_t>(nowMs - runtime.activeSinceMs) < plan.continueTimeMs) {
            runtime.active = false;
            runtime.firedForActive = false;
            runtime.lockedUntilMs = nowMs + plan.lockTimeMs;
            return true;
        }
        runtime.active = false;
        runtime.firedForActive = false;
        return false;
    }

    if (!runtime.active) {
        runtime.active = true;
        runtime.activeSinceMs = nowMs;
        runtime.firedForActive = false;
    }

    if (runtime.firedForActive) return false;

    if (plan.continueTimeMs == 0 ||
        (plan.continueMode == 1 &&
         static_cast<uint32_t>(nowMs - runtime.activeSinceMs) >= plan.continueTimeMs)) {
        runtime.firedForActive = true;
        runtime.lockedUntilMs = nowMs + plan.lockTimeMs;
        return true;
    }
    return false;
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
