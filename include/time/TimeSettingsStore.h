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
        if (!validUtcOffsetMinutes(settings.utcOffsetMinutes)) return false;

        settings.dst.enabled = prefs_.getBool(kDstEnabledKey, false);
        settings.dst.biasMinutes = prefs_.getUChar(kDstBiasKey, 0);
        settings.dst.start.month = prefs_.getUChar(kDstStartMonthKey, 0);
        settings.dst.start.week = prefs_.getUChar(kDstStartWeekKey, 0);
        settings.dst.start.weekday = prefs_.getUChar(kDstStartWeekdayKey, 0);
        settings.dst.start.minuteOfDay = prefs_.getUShort(kDstStartMinuteKey, 0);
        settings.dst.end.month = prefs_.getUChar(kDstEndMonthKey, 0);
        settings.dst.end.week = prefs_.getUChar(kDstEndWeekKey, 0);
        settings.dst.end.weekday = prefs_.getUChar(kDstEndWeekdayKey, 0);
        settings.dst.end.minuteOfDay = prefs_.getUShort(kDstEndMinuteKey, 0);
        return validDstSettings(settings.dst);
    }

    bool save(const Settings& settings) {
        if (!validUtcOffsetMinutes(settings.utcOffsetMinutes) || !validDstSettings(settings.dst)) return false;
        if (prefs_.putShort(kTimezoneKey, settings.utcOffsetMinutes) != sizeof(int16_t)) return false;
        if (prefs_.putBool(kDstEnabledKey, settings.dst.enabled) != sizeof(bool)) return false;
        if (prefs_.putUChar(kDstBiasKey, settings.dst.biasMinutes) != sizeof(uint8_t)) return false;
        if (prefs_.putUChar(kDstStartMonthKey, settings.dst.start.month) != sizeof(uint8_t)) return false;
        if (prefs_.putUChar(kDstStartWeekKey, settings.dst.start.week) != sizeof(uint8_t)) return false;
        if (prefs_.putUChar(kDstStartWeekdayKey, settings.dst.start.weekday) != sizeof(uint8_t)) return false;
        if (prefs_.putUShort(kDstStartMinuteKey, settings.dst.start.minuteOfDay) != sizeof(uint16_t)) return false;
        if (prefs_.putUChar(kDstEndMonthKey, settings.dst.end.month) != sizeof(uint8_t)) return false;
        if (prefs_.putUChar(kDstEndWeekKey, settings.dst.end.week) != sizeof(uint8_t)) return false;
        if (prefs_.putUChar(kDstEndWeekdayKey, settings.dst.end.weekday) != sizeof(uint8_t)) return false;
        return prefs_.putUShort(kDstEndMinuteKey, settings.dst.end.minuteOfDay) == sizeof(uint16_t);
    }

    void clear() {
        prefs_.clear();
    }

private:
    static constexpr const char* kTimezoneKey = "utc_min";
    static constexpr const char* kDstEnabledKey = "dst_en";
    static constexpr const char* kDstBiasKey = "dst_bias";
    static constexpr const char* kDstStartMonthKey = "dst_sm";
    static constexpr const char* kDstStartWeekKey = "dst_sw";
    static constexpr const char* kDstStartWeekdayKey = "dst_sd";
    static constexpr const char* kDstStartMinuteKey = "dst_st";
    static constexpr const char* kDstEndMonthKey = "dst_em";
    static constexpr const char* kDstEndWeekKey = "dst_ew";
    static constexpr const char* kDstEndWeekdayKey = "dst_ed";
    static constexpr const char* kDstEndMinuteKey = "dst_et";
    mutable Preferences prefs_;
};

} // namespace multibus::time
