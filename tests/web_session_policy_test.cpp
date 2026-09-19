#include "core/WebSessionPolicy.h"

#include <cassert>

using multibus::WebSessionPolicy;

int main() {
    assert(!WebSessionPolicy::expired(1000, 2000, 3000));
    assert(WebSessionPolicy::expired(
        1000, 2000, 1000 + WebSessionPolicy::kMaxLifetimeMs + 1U));
    assert(WebSessionPolicy::expired(
        1000, 2000, 2000 + WebSessionPolicy::kIdleTimeoutMs + 1U));

    const uint32_t nearWrap = 0xfffffff0U;
    assert(!WebSessionPolicy::expired(nearWrap, nearWrap, 0x00000010U));

    const char* token = "0123456789abcdef0123456789abcdef";
    assert(WebSessionPolicy::hasSessionCookie(
        "MBSESSION=0123456789abcdef0123456789abcdef", token));
    assert(WebSessionPolicy::hasSessionCookie(
        "theme=dark; MBSESSION=0123456789abcdef0123456789abcdef; x=1", token));
    assert(WebSessionPolicy::hasSessionCookie(
        "theme=dark;\tMBSESSION = 0123456789abcdef0123456789abcdef ; x=1", token));

    assert(!WebSessionPolicy::hasSessionCookie(nullptr, token));
    assert(!WebSessionPolicy::hasSessionCookie("", token));
    assert(!WebSessionPolicy::hasSessionCookie(
        "XMBSESSION=0123456789abcdef0123456789abcdef", token));
    assert(!WebSessionPolicy::hasSessionCookie(
        "MBSESSION=0123456789abcdef0123456789abcdef0", token));
    assert(!WebSessionPolicy::hasSessionCookie(
        "MBSESSION=00123456789abcdef0123456789abcde", token));
    assert(!WebSessionPolicy::hasSessionCookie("MBSESSION=", token));

    assert(WebSessionPolicy::csrfMatches(
        "abcdef0123456789abcdef0123456789",
        "abcdef0123456789abcdef0123456789"));
    assert(!WebSessionPolicy::csrfMatches(
        "abcdef0123456789abcdef0123456788",
        "abcdef0123456789abcdef0123456789"));
    assert(!WebSessionPolicy::csrfMatches("", ""));
    assert(!WebSessionPolicy::csrfMatches("short", "longer"));

    return 0;
}
