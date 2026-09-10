#pragma once

#include <Arduino.h>
#include <Esp.h>

namespace multibus {

inline uint64_t hardwareMac48() {
    return ESP.getEfuseMac() & 0x0000FFFFFFFFFFFFULL;
}

inline String deviceSuffix() {
    const uint64_t mac = hardwareMac48();
    char buf[7];
    snprintf(buf, sizeof(buf), "%06llX", static_cast<unsigned long long>(mac & 0xFFFFFFULL));
    return String(buf);
}

inline String devEui() {
    const uint64_t mac = hardwareMac48();
    const uint8_t bytes[8] = {
        0x02,
        static_cast<uint8_t>((mac >> 40) & 0xff),
        static_cast<uint8_t>((mac >> 32) & 0xff),
        static_cast<uint8_t>((mac >> 24) & 0xff),
        static_cast<uint8_t>((mac >> 16) & 0xff),
        static_cast<uint8_t>((mac >> 8) & 0xff),
        static_cast<uint8_t>(mac & 0xff),
        0x01,
    };

    char out[17];
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        snprintf(out + i * 2, 3, "%02X", bytes[i]);
    }
    out[16] = '\0';
    return String(out);
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
