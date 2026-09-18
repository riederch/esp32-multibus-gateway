#include <assert.h>
#include <stdint.h>

#include "core/LoginThrottle.h"

using multibus::LoginThrottle;

static void failFive(LoginThrottle& throttle, uint32_t now) {
    for (int i = 0; i < 5; ++i) throttle.recordFailure(now);
}

static void testProgressiveBackoff() {
    LoginThrottle throttle;
    assert(throttle.allowed(1000));

    failFive(throttle, 1000);
    assert(!throttle.allowed(1001));
    assert(throttle.retryAfterSeconds(1000) == 30);
    assert(throttle.allowed(31000));

    failFive(throttle, 31000);
    assert(throttle.retryAfterSeconds(31000) == 60);
    assert(!throttle.allowed(90999));
    assert(throttle.allowed(91000));

    failFive(throttle, 91000);
    assert(throttle.retryAfterSeconds(91000) == 120);
}

static void testSuccessResetsPenalty() {
    LoginThrottle throttle;
    failFive(throttle, 0);
    assert(throttle.lockTier() == 1);
    throttle.recordSuccess();
    assert(throttle.lockTier() == 0);
    assert(throttle.allowed(1));
    failFive(throttle, 1);
    assert(throttle.retryAfterSeconds(1) == 30);
}

static void testLockedAttemptsDoNotEscalate() {
    LoginThrottle throttle;
    failFive(throttle, 100);
    const uint8_t tier = throttle.lockTier();
    for (int i = 0; i < 20; ++i) throttle.recordFailure(200);
    assert(throttle.lockTier() == tier);
    assert(throttle.retryAfterSeconds(200) == 30);
}

int main() {
    testProgressiveBackoff();
    testSuccessResetsPenalty();
    testLockedAttemptsDoNotEscalate();
    return 0;
}
