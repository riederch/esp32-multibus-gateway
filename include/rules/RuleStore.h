#pragma once

#include <Preferences.h>
#include <stdio.h>

#include "RuleState.h"

namespace multibus::rules {

class RuleStore {
public:
    bool begin() {
        return prefs_.begin("mbrules", false);
    }

    void end() {
        prefs_.end();
    }

    bool load(RuleState& state) const {
        state = RuleState{};
        for (uint8_t id = 1; id <= kRuleCount; ++id) {
            RuleRecord* record = state.rule(id);
            if (record == nullptr) return false;

            char key[8];
            snprintf(key, sizeof(key), "r%02u", static_cast<unsigned>(id));
            const size_t length = prefs_.getBytesLength(key);
            if (length == 0) continue;
            if (length != sizeof(RuleRecord)) return false;
            if (prefs_.getBytes(key, record, sizeof(RuleRecord)) != sizeof(RuleRecord)) return false;

            for (const auto& frame : record->frames) {
                if (frame.length > kMaxRuleFrameLength) return false;
            }
        }
        return true;
    }

    bool saveRule(uint8_t ruleId, const RuleRecord& record) {
        if (ruleId < 1 || ruleId > kRuleCount) return false;
        char key[8];
        snprintf(key, sizeof(key), "r%02u", static_cast<unsigned>(ruleId));
        return prefs_.putBytes(key, &record, sizeof(record)) == sizeof(record);
    }

    bool removeRule(uint8_t ruleId) {
        if (ruleId < 1 || ruleId > kRuleCount) return false;
        char key[8];
        snprintf(key, sizeof(key), "r%02u", static_cast<unsigned>(ruleId));
        return prefs_.remove(key);
    }

    void clear() {
        prefs_.clear();
    }

private:
    mutable Preferences prefs_;
};

} // namespace multibus::rules
