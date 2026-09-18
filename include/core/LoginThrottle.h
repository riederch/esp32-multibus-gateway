#pragma once

#include <stdint.h>

namespace multibus {

class LoginThrottle {
public:
    bool allowed(uint32_t nowMs) const {
        return lockUntilMs_ == 0 ||
               static_cast<int32_t>(nowMs - lockUntilMs_) >= 0;
    }

    uint32_t retryAfterSeconds(uint32_t nowMs) const {
        if (allowed(nowMs)) return 0;
        const uint32_t remainingMs = lockUntilMs_ - nowMs;
        return (remainingMs + 999U) / 1000U;
    }

    void recordFailure(uint32_t nowMs) {
        if (!allowed(nowMs)) return;

        if (failureCount_ < UINT8_MAX) ++failureCount_;
        if (failureCount_ < kFailuresBeforeLock) return;

        const uint8_t tier = lockTier_ < kMaxLockTier ? lockTier_ : kMaxLockTier;
        uint32_t lockMs = kInitialLockMs << tier;
        if (lockMs > kMaximumLockMs) lockMs = kMaximumLockMs;

        lockUntilMs_ = nowMs + lockMs;
        failureCount_ = 0;
        if (lockTier_ < kMaxLockTier) ++lockTier_;
    }

    void recordSuccess() {
        failureCount_ = 0;
        lockTier_ = 0;
        lockUntilMs_ = 0;
    }

    uint8_t lockTier() const { return lockTier_; }

private:
    static constexpr uint8_t kFailuresBeforeLock = 5;
    static constexpr uint8_t kMaxLockTier = 5;
    static constexpr uint32_t kInitialLockMs = 30000UL;
    static constexpr uint32_t kMaximumLockMs = 15UL * 60UL * 1000UL;

    uint8_t failureCount_ = 0;
    uint8_t lockTier_ = 0;
    uint32_t lockUntilMs_ = 0;
};

} // namespace multibus
