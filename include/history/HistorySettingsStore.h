#pragma once

#include <Preferences.h>
#include "HistorySettings.h"

namespace multibus::history {

class SettingsStore {
public:
    bool begin() {
        return prefs_.begin("mbhistory", false);
    }

    void end() {
        prefs_.end();
    }

    bool load(Settings& settings) const {
        settings = Settings{};
        settings.storageEnabled = prefs_.getBool(kStorageKey, false);
        settings.retransmissionEnabled = prefs_.getBool(kRetransmissionKey, false);
        settings.retransmissionIntervalSeconds =
            prefs_.getUShort(kIntervalKey, kDefaultRetransmissionIntervalSeconds);
        settings.retrievabilityIntervalSeconds =
            prefs_.getUShort(kRetrievabilityIntervalKey, kDefaultRetrievabilityIntervalSeconds);
        return validSettings(settings);
    }

    bool save(const Settings& settings) {
        if (!validSettings(settings)) return false;
        if (prefs_.putBool(kStorageKey, settings.storageEnabled) != sizeof(bool)) return false;
        if (prefs_.putBool(kRetransmissionKey, settings.retransmissionEnabled) != sizeof(bool)) return false;
        if (prefs_.putUShort(kIntervalKey, settings.retransmissionIntervalSeconds) != sizeof(uint16_t)) return false;
        return prefs_.putUShort(kRetrievabilityIntervalKey, settings.retrievabilityIntervalSeconds) == sizeof(uint16_t);
    }

    void clear() {
        prefs_.clear();
    }

private:
    static constexpr const char* kStorageKey = "store";
    static constexpr const char* kRetransmissionKey = "retrans";
    static constexpr const char* kIntervalKey = "retry_s";
    static constexpr const char* kRetrievabilityIntervalKey = "query_s";
    mutable Preferences prefs_;
};

} // namespace multibus::history
