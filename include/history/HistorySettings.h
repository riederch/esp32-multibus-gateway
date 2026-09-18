#pragma once

#include <stdint.h>

namespace multibus::history {

constexpr uint16_t kDefaultRetransmissionIntervalSeconds = 600;
constexpr uint16_t kMinRetransmissionIntervalSeconds = 30;
constexpr uint16_t kMaxRetransmissionIntervalSeconds = 1200;
constexpr uint16_t kDefaultRetrievabilityIntervalSeconds = 60;
constexpr uint16_t kMinRetrievabilityIntervalSeconds = 30;
constexpr uint16_t kMaxRetrievabilityIntervalSeconds = 1200;

struct Settings {
    bool storageEnabled = false;
    bool retransmissionEnabled = false;
    uint16_t retransmissionIntervalSeconds = kDefaultRetransmissionIntervalSeconds;
    uint16_t retrievabilityIntervalSeconds = kDefaultRetrievabilityIntervalSeconds;
};

inline bool validSettings(const Settings& settings) {
    return settings.retransmissionIntervalSeconds >= kMinRetransmissionIntervalSeconds &&
           settings.retransmissionIntervalSeconds <= kMaxRetransmissionIntervalSeconds &&
           settings.retrievabilityIntervalSeconds >= kMinRetrievabilityIntervalSeconds &&
           settings.retrievabilityIntervalSeconds <= kMaxRetrievabilityIntervalSeconds;
}

} // namespace multibus::history
