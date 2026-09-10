#pragma once

#include <Arduino.h>
#include <ESP.h>

namespace multibus {

inline String deviceSuffix() {
    const uint64_t mac = ESP.getEfuseMac();
    char buf[7];
    snprintf(buf, sizeof(buf), "%06llX", static_cast<unsigned long long>(mac & 0xFFFFFFULL));
    return String(buf);
}

inline String defaultHostname() {
    String value = "multibus-" + deviceSuffix();
    value.toLowerCase();
    return value;
}

inline String defaultApSsid() {
    return "MultiBus-" + deviceSuffix();
}

} // namespace multibus
