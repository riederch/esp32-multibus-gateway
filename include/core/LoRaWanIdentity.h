#pragma once

#include <Arduino.h>
#include <esp_system.h>
#include "DeviceConfig.h"

namespace multibus {

inline bool isHexString(const String& value, size_t length) {
    if (value.length() != length) return false;
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        const bool hex = (c >= '0' && c <= '9') ||
                         (c >= 'a' && c <= 'f') ||
                         (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

inline String generateAppKey() {
    static constexpr char hex[] = "0123456789ABCDEF";
    uint8_t key[16];
    esp_fill_random(key, sizeof(key));

    char out[33];
    for (size_t i = 0; i < sizeof(key); ++i) {
        out[i * 2] = hex[key[i] >> 4];
        out[i * 2 + 1] = hex[key[i] & 0x0F];
    }
    out[32] = '\0';
    return String(out);
}

inline bool validateLoRaWanConfig(const LoRaWanConfig& config) {
    if (!isHexString(config.joinEui, 16)) return false;
    if (!isHexString(config.appKey, 32)) return false;
    if (config.extensionFPort == 0 || config.extensionFPort == LORAWAN_COMPATIBILITY_FPORT) return false;
    return true;
}

inline bool ensureLoRaWanConfig(LoRaWanConfig& config) {
    bool changed = false;

    if (!isHexString(config.joinEui, 16)) {
        config.joinEui = MULTIBUS_DEFAULT_JOIN_EUI;
        changed = true;
    }
    if (!isHexString(config.appKey, 32)) {
        config.appKey = generateAppKey();
        changed = true;
    }
    if (config.extensionFPort == 0 || config.extensionFPort == LORAWAN_COMPATIBILITY_FPORT) {
        config.extensionFPort = LORAWAN_DEFAULT_EXTENSION_FPORT;
        changed = true;
    }

    return changed;
}

} // namespace multibus
