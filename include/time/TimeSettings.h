#pragma once

#include <stdint.h>

namespace multibus::time {

struct DstTransition {
    uint8_t month = 0;
    uint8_t week = 0;
    uint8_t weekday = 0;
    uint16_t minuteOfDay = 0;
};

struct DstSettings {
    bool enabled = false;
    uint8_t biasMinutes = 0;
    DstTransition start;
    DstTransition end;
};

struct Settings {
    int16_t utcOffsetMinutes = 0;
    DstSettings dst;
};

inline bool validUtcOffsetMinutes(int16_t minutes) {
    return minutes >= -720 && minutes <= 840;
}

inline bool validDstTransition(const DstTransition& transition) {
    return transition.month >= 1 && transition.month <= 12 &&
           transition.week >= 1 && transition.week <= 5 &&
           transition.weekday >= 1 && transition.weekday <= 7 &&
           transition.minuteOfDay <= 1439;
}

inline bool validDstSettings(const DstSettings& settings) {
    if (!settings.enabled) return settings.biasMinutes <= 120;
    return settings.biasMinutes >= 1 && settings.biasMinutes <= 120 &&
           validDstTransition(settings.start) &&
           validDstTransition(settings.end);
}

} // namespace multibus::time
