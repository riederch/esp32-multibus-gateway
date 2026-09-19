#include "core/WebTransportPolicy.h"

#include <cassert>
#include <cstring>

using multibus::WebTransportMode;
using multibus::WebTransportPolicy;

int main() {
    assert(WebTransportPolicy::authenticatedAdministrationAllowed(
        WebTransportMode::CommissioningHttp));
    assert(!WebTransportPolicy::authenticatedAdministrationAllowed(
        WebTransportMode::StationHttpLegacy));
    assert(WebTransportPolicy::authenticatedAdministrationAllowed(
        WebTransportMode::StationHttps));

    assert(!WebTransportPolicy::redirectToHttps(
        WebTransportMode::CommissioningHttp));
    assert(WebTransportPolicy::redirectToHttps(
        WebTransportMode::StationHttpLegacy));
    assert(!WebTransportPolicy::redirectToHttps(
        WebTransportMode::StationHttps));

    assert(!WebTransportPolicy::secureSessionCookie(
        WebTransportMode::CommissioningHttp));
    assert(!WebTransportPolicy::secureSessionCookie(
        WebTransportMode::StationHttpLegacy));
    assert(WebTransportPolicy::secureSessionCookie(
        WebTransportMode::StationHttps));

    assert(WebTransportPolicy::administrationPort(
        WebTransportMode::CommissioningHttp) == 80U);
    assert(WebTransportPolicy::administrationPort(
        WebTransportMode::StationHttpLegacy) == 80U);
    assert(WebTransportPolicy::administrationPort(
        WebTransportMode::StationHttps) == 443U);

    assert(std::strcmp(WebTransportPolicy::mdnsService(
                           WebTransportMode::CommissioningHttp),
                       "http") == 0);
    assert(std::strcmp(WebTransportPolicy::mdnsService(
                           WebTransportMode::StationHttpLegacy),
                       "http") == 0);
    assert(std::strcmp(WebTransportPolicy::mdnsService(
                           WebTransportMode::StationHttps),
                       "https") == 0);

    return 0;
}
