#pragma once

#include <cstdint>

namespace multibus {

enum class WebTransportMode : uint8_t {
    CommissioningHttp,
    StationHttpLegacy,
    StationHttps,
};

struct WebTransportPolicy {
    static constexpr bool authenticatedAdministrationAllowed(WebTransportMode mode) {
        return mode == WebTransportMode::CommissioningHttp ||
               mode == WebTransportMode::StationHttps;
    }

    static constexpr bool redirectToHttps(WebTransportMode mode) {
        return mode == WebTransportMode::StationHttpLegacy;
    }

    static constexpr bool secureSessionCookie(WebTransportMode mode) {
        return mode == WebTransportMode::StationHttps;
    }

    static constexpr uint16_t administrationPort(WebTransportMode mode) {
        return mode == WebTransportMode::StationHttps ? 443U : 80U;
    }

    static constexpr const char* mdnsService(WebTransportMode mode) {
        return mode == WebTransportMode::StationHttps ? "https" : "http";
    }
};

} // namespace multibus
