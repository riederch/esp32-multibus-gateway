#pragma once

#include <stdint.h>
#include "TimeSettings.h"

namespace multibus::time {

struct LocalDateTime {
    int year = 1970;
    uint8_t month = 1;
    uint8_t day = 1;
    uint8_t weekday = 4; // ISO: Monday=1..Sunday=7
    uint8_t hour = 0;
    uint8_t minute = 0;
};

inline int64_t daysFromCivil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153U * (month + (month > 2 ? static_cast<unsigned>(-3) : 9U)) + 2U) / 5U + day - 1U;
    const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    return static_cast<int64_t>(era) * 146097LL + static_cast<int64_t>(doe) - 719468LL;
}

inline bool leapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

inline uint8_t daysInMonth(int year, uint8_t month) {
    static constexpr uint8_t kDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && leapYear(year)) return 29;
    return (month >= 1 && month <= 12) ? kDays[month - 1U] : 0;
}

inline uint8_t isoWeekday(int year, uint8_t month, uint8_t day) {
    const int64_t days = daysFromCivil(year, month, day);
    int value = static_cast<int>((days + 3LL) % 7LL);
    if (value < 0) value += 7;
    return static_cast<uint8_t>(value + 1);
}

inline uint8_t transitionDayOfMonth(int year, const DstTransition& transition) {
    const uint8_t dim = daysInMonth(year, transition.month);
    if (dim == 0 || !validDstTransition(transition)) return 0;

    if (transition.week < 5) {
        const uint8_t firstWeekday = isoWeekday(year, transition.month, 1);
        const uint8_t delta = static_cast<uint8_t>(
            (7 + transition.weekday - firstWeekday) % 7);
        const uint16_t day = static_cast<uint16_t>(1U + delta + (transition.week - 1U) * 7U);
        return day <= dim ? static_cast<uint8_t>(day) : 0;
    }

    const uint8_t lastWeekday = isoWeekday(year, transition.month, dim);
    const uint8_t delta = static_cast<uint8_t>(
        (7 + lastWeekday - transition.weekday) % 7);
    return static_cast<uint8_t>(dim - delta);
}

inline int64_t localSecondsForTransition(int year, const DstTransition& transition) {
    const uint8_t day = transitionDayOfMonth(year, transition);
    if (day == 0) return INT64_MIN;
    return daysFromCivil(year, transition.month, day) * 86400LL +
           static_cast<int64_t>(transition.minuteOfDay) * 60LL;
}

inline LocalDateTime dateTimeFromEpoch(int64_t seconds) {
    int64_t days = seconds / 86400LL;
    int64_t sod = seconds % 86400LL;
    if (sod < 0) {
        sod += 86400LL;
        --days;
    }

    int z = static_cast<int>(days + 719468LL);
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
    int year = static_cast<int>(yoe) + era * 400;
    const unsigned doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);
    const unsigned mp = (5U * doy + 2U) / 153U;
    const unsigned day = doy - (153U * mp + 2U) / 5U + 1U;
    const unsigned month = mp + (mp < 10U ? 3U : static_cast<unsigned>(-9));
    year += month <= 2U;

    LocalDateTime result;
    result.year = year;
    result.month = static_cast<uint8_t>(month);
    result.day = static_cast<uint8_t>(day);
    result.weekday = isoWeekday(year, result.month, result.day);
    result.hour = static_cast<uint8_t>(sod / 3600LL);
    result.minute = static_cast<uint8_t>((sod % 3600LL) / 60LL);
    return result;
}

inline bool dstActiveForStandardLocalSeconds(int64_t standardLocalSeconds,
                                             const Settings& settings) {
    if (!settings.dst.enabled || !validDstSettings(settings.dst)) return false;

    const LocalDateTime standard = dateTimeFromEpoch(standardLocalSeconds);
    const int64_t start = localSecondsForTransition(standard.year, settings.dst.start);
    int64_t end = localSecondsForTransition(standard.year, settings.dst.end);
    if (start == INT64_MIN || end == INT64_MIN) return false;

    // The end transition is expressed in daylight wall time, so convert it
    // to standard-local seconds before comparing against the standard clock.
    end -= static_cast<int64_t>(settings.dst.biasMinutes) * 60LL;

    if (start < end) {
        return standardLocalSeconds >= start && standardLocalSeconds < end;
    }
    return standardLocalSeconds >= start || standardLocalSeconds < end;
}

inline LocalDateTime localDateTime(uint32_t unixSeconds, const Settings& settings) {
    int64_t localSeconds = static_cast<int64_t>(unixSeconds) +
                           static_cast<int64_t>(settings.utcOffsetMinutes) * 60LL;
    if (dstActiveForStandardLocalSeconds(localSeconds, settings)) {
        localSeconds += static_cast<int64_t>(settings.dst.biasMinutes) * 60LL;
    }
    return dateTimeFromEpoch(localSeconds);
}

} // namespace multibus::time
