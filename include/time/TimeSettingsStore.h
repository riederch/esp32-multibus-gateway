#pragma once

#include <Preferences.h>
#include "TimeSettings.h"

namespace multibus::time {

class SettingsStore {
public:
    bool begin() {
        return prefs_.begin("mbtime", false);
    }

    void end() {
        prefs_.end();
    }

    bool load(Settings& settings) const {
        if (!prefs_.isKey(kTimezoneKey)) {
            settings = Settings{};
            return true;
        }
        settings.utcOffsetMinutes = prefs_.getShort(kTimezoneKey, 0);
        return validUtcOffsetMinutes(settings.utcOffsetMinutes);
    }

    bool save(const Settings& settings) {
        if (!validUtcOffsetMinutes(settings.utcOffsetMinutes)) return false;
        return prefs_.putShort(kTimezoneKey, settings.utcOffsetMinutes) == sizeof(int16_t);
    }

    void clear() {
        prefs_.clear();
    }

private:
    static constexpr const char* kTimezoneKey = "utc_min";
    mutable Preferences prefs_;
};

} // namespace multibus::time
