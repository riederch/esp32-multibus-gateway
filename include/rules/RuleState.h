#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace multibus::rules {

constexpr size_t kRuleCount = 16;
constexpr size_t kRuleFrameSlots = 4;
constexpr size_t kMaxRuleFrameLength = 57;

struct StoredFrame {
    uint8_t length = 0;
    uint8_t data[kMaxRuleFrameLength] = {0};

    bool set(const uint8_t* source, size_t sourceLength) {
        if (source == nullptr || sourceLength < 4 || sourceLength > kMaxRuleFrameLength) return false;
        length = static_cast<uint8_t>(sourceLength);
        memcpy(data, source, sourceLength);
        return true;
    }

    void clear() {
        length = 0;
        memset(data, 0, sizeof(data));
    }

    bool present() const { return length != 0; }
};

struct RuleRecord {
    bool enabled = false;
    StoredFrame frames[kRuleFrameSlots];

    bool configured() const {
        for (const auto& frame : frames) if (frame.present()) return true;
        return false;
    }

    void clear() {
        enabled = false;
        for (auto& frame : frames) frame.clear();
    }
};

class RuleState {
public:
    RuleRecord* rule(uint8_t ruleId) {
        if (ruleId < 1 || ruleId > kRuleCount) return nullptr;
        return &rules_[ruleId - 1U];
    }

    const RuleRecord* rule(uint8_t ruleId) const {
        if (ruleId < 1 || ruleId > kRuleCount) return nullptr;
        return &rules_[ruleId - 1U];
    }

    bool applyMask(uint16_t mask, uint8_t operation) {
        if (mask == 0 || operation < 1 || operation > 3) return false;
        for (uint8_t bit = 0; bit < kRuleCount; ++bit) {
            if ((mask & (static_cast<uint16_t>(1U) << bit)) == 0) continue;
            RuleRecord& record = rules_[bit];
            if (operation == 1) record.enabled = true;
            else if (operation == 2) record.enabled = false;
            else record.clear();
        }
        return true;
    }

private:
    RuleRecord rules_[kRuleCount];
};

} // namespace multibus::rules
