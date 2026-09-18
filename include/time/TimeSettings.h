#pragma once

#include <stdint.h>

namespace multibus::time {

struct Settings {
    int16_t utcOffsetMinutes = 0;
};

inline bool validUtcOffsetMinutes(int16_t minutes) {
    return minutes >= -720 && minutes <= 840 && (minutes % 15) == 0;
}

} // namespace multibus::time
