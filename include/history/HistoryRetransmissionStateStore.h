#pragma once

#include <Preferences.h>

#include "HistoryRetransmissionState.h"

namespace multibus::history {

class RetransmissionStateStore {
public:
    bool begin() {
        return prefs_.begin("mbhistrtx", false);
    }

    void end() {
        prefs_.end();
    }

    bool load(RetransmissionState& state) const {
        state = RetransmissionState{};
        state.pending = prefs_.getBool(kPendingKey, false);
        state.lostAtUnix = prefs_.getUInt(kLostAtKey, 0);
        if (!state.pending) state.lostAtUnix = 0;
        return !state.pending || state.lostAtUnix != 0;
    }

    bool save(const RetransmissionState& state) {
        if (state.pending && state.lostAtUnix == 0) return false;
        if (prefs_.putBool(kPendingKey, state.pending) != sizeof(bool)) return false;
        return prefs_.putUInt(kLostAtKey, state.pending ? state.lostAtUnix : 0) == sizeof(uint32_t);
    }

    void clear() {
        prefs_.clear();
    }

private:
    static constexpr const char* kPendingKey = "pending";
    static constexpr const char* kLostAtKey = "lost_at";
    mutable Preferences prefs_;
};

} // namespace multibus::history
