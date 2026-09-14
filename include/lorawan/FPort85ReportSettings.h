#pragma once

#include <stddef.h>
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

inline bool decodeReportIntervalCommand(const uint8_t* payload,
                                        size_t length,
                                        ReportIntervalCommand& command,
                                        size_t& consumed) {
    consumed = 0;
    if (payload == nullptr || length < 4) return false;
    if (payload[0] != 0xff || payload[1] != 0x03) return false;

    ReportIntervalSettings settings;
    settings.seconds = static_cast<uint16_t>(payload[2]) |
                       (static_cast<uint16_t>(payload[3]) << 8U);
    if (!validReportIntervalSettings(settings)) return false;

    command = ReportIntervalCommand{};
    command.settings = settings;
    consumed = 4;
    return true;
}

} // namespace multibus::lorawan
