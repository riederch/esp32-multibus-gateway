#pragma once

#include <stdint.h>

namespace multibus::history {

constexpr uint16_t kDefaultRetransmissionIntervalSeconds = 600;
constexpr uint16_t kMinRetransmissionIntervalSeconds = 30;
constexpr uint16_t kMaxRetransmissionIntervalSeconds = 1200;

struct Settings {
    bool storageEnabled = false;
    bool retransmissionEnabled = false;
    uint16_t retransmissionIntervalSeconds = kDefaultRetransmissionIntervalSeconds;
};

inline bool validSettings(const Settings& settings) {
    return settings.retransmissionIntervalSeconds >= kMinRetransmissionIntervalSeconds &&
           settings.retransmissionIntervalSeconds <= kMaxRetransmissionIntervalSeconds;
}

} // namespace multibus::history
