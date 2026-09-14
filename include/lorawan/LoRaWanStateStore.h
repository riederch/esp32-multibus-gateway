#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <cstring>
#include "core/DeviceConfig.h"

namespace multibus::lorawan {

class LoRaWanStateStore {
public:
    bool begin() {
        if (open_) return true;
        open_ = prefs_.begin("multibus", false);
        return open_;
    }

    void end() {
        if (!open_) return;
        prefs_.end();
        open_ = false;
    }

    static uint32_t identityFingerprint(const LoRaWanConfig& config, const String& devEui) {
        uint32_t hash = 2166136261UL;
        mix(hash, "lorawan-state-v1");
        mix(hash, devEui);
        mix(hash, config.joinEui);
        mix(hash, config.appKey);
        return hash;
    }

    bool load(uint32_t identity,
              uint8_t (&nonces)[RADIOLIB_LORAWAN_NONCES_BUF_SIZE],
              bool& hasNonces,
              uint8_t (&session)[RADIOLIB_LORAWAN_SESSION_BUF_SIZE],
              bool& hasSession) {
        hasNonces = false;
        hasSession = false;
        if (!open_) return false;

        if (prefs_.getUInt(kIdentityKey, 0) != identity) return true;

        if (prefs_.getBytesLength(kNoncesKey) == RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
            hasNonces = prefs_.getBytes(kNoncesKey, nonces, sizeof(nonces)) == sizeof(nonces);
        }
        if (prefs_.getBytesLength(kSessionKey) == RADIOLIB_LORAWAN_SESSION_BUF_SIZE) {
            hasSession = prefs_.getBytes(kSessionKey, session, sizeof(session)) == sizeof(session);
        }

        if (hasSession) {
            memcpy(lastSession_, session, sizeof(lastSession_));
            lastSessionValid_ = true;
        } else {
            lastSessionValid_ = false;
        }
        return true;
    }

    bool saveNonces(uint32_t identity, const uint8_t* nonces) {
        if (!open_ || nonces == nullptr) return false;
        if (!ensureIdentity(identity)) return false;
        return prefs_.putBytes(kNoncesKey, nonces, RADIOLIB_LORAWAN_NONCES_BUF_SIZE) ==
               RADIOLIB_LORAWAN_NONCES_BUF_SIZE;
    }

    bool saveSessionIfChanged(uint32_t identity, const uint8_t* session) {
        if (!open_ || session == nullptr) return false;
        if (lastSessionValid_ && memcmp(lastSession_, session, sizeof(lastSession_)) == 0) return true;
        if (!ensureIdentity(identity)) return false;
        if (prefs_.putBytes(kSessionKey, session, RADIOLIB_LORAWAN_SESSION_BUF_SIZE) !=
            RADIOLIB_LORAWAN_SESSION_BUF_SIZE) {
            return false;
        }
        memcpy(lastSession_, session, sizeof(lastSession_));
        lastSessionValid_ = true;
        return true;
    }

    bool clearSession(uint32_t identity) {
        if (!open_) return false;
        if (prefs_.getUInt(kIdentityKey, 0) != identity) {
            lastSessionValid_ = false;
            return true;
        }
        if (prefs_.isKey(kSessionKey) && !prefs_.remove(kSessionKey)) return false;
        lastSessionValid_ = false;
        return true;
    }

private:
    static constexpr const char* kIdentityKey = "lw_state_id";
    static constexpr const char* kNoncesKey = "lw_nonces";
    static constexpr const char* kSessionKey = "lw_session";

    static void mix(uint32_t& hash, const char* value) {
        if (value == nullptr) return;
        while (*value != '\0') {
            hash ^= static_cast<uint8_t>(*value++);
            hash *= 16777619UL;
        }
        hash ^= 0xffU;
        hash *= 16777619UL;
    }

    static void mix(uint32_t& hash, const String& value) {
        for (size_t i = 0; i < value.length(); ++i) {
            hash ^= static_cast<uint8_t>(value.charAt(i));
            hash *= 16777619UL;
        }
        hash ^= 0xffU;
        hash *= 16777619UL;
    }

    bool ensureIdentity(uint32_t identity) {
        const uint32_t stored = prefs_.getUInt(kIdentityKey, 0);
        if (stored == identity) return true;

        prefs_.remove(kNoncesKey);
        prefs_.remove(kSessionKey);
        lastSessionValid_ = false;
        return prefs_.putUInt(kIdentityKey, identity) == sizeof(uint32_t);
    }

    Preferences prefs_;
    bool open_ = false;
    bool lastSessionValid_ = false;
    uint8_t lastSession_[RADIOLIB_LORAWAN_SESSION_BUF_SIZE] = {0};
};

} // namespace multibus::lorawan
