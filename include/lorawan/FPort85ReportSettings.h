#pragma once

#include <stdint.h>

namespace multibus::lorawan {

constexpr uint16_t kDefaultReportIntervalSeconds = 20U * 60U;
constexpr uint16_t kMinReportIntervalSeconds = 60U;
constexpr uint16_t kMaxReportIntervalSeconds = 1080U * 60U;

struct ReportIntervalSettings {
    uint16_t seconds = kDefaultReportIntervalSeconds;
};

inline bool validReportIntervalSeconds(uint16_t seconds) {
    return seconds >= kMinReportIntervalSeconds && seconds <= kMaxReportIntervalSeconds;
}

inline bool validReportIntervalSettings(const ReportIntervalSettings& settings) {
    return validReportIntervalSeconds(settings.seconds);
}

struct ReportIntervalCommand {
    ReportIntervalSettings settings;
};

} // namespace multibus::lorawan
