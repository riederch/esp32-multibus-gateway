#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <mbedtls/md.h>

namespace multibus {

class SecurityStore {
public:
    bool begin() {
        if (!prefs_.begin("security", false)) return false;

        if (!prefs_.isKey("ap_pass")) {
            const String generated = generateSecret(12);
            prefs_.putString("ap_pass", generated);
        }

        if (!prefs_.isKey("admin_hash")) {
            initialAdminPassword_ = generateSecret(16);
            const String salt = generateHex(16);
            const String hash = pbkdf2Sha256(initialAdminPassword_, salt, kIterations);
            if (hash.isEmpty()) return false;
            prefs_.putString("admin_salt", salt);
            prefs_.putString("admin_hash", hash);
            prefs_.putBool("admin_init", false);
        }

        return true;
    }

    void end() { prefs_.end(); }

    String apPassword() const {
        return prefs_.getString("ap_pass", "");
    }

    bool adminInitialized() const {
        return prefs_.getBool("admin_init", false);
    }

    bool hasInitialAdminPasswordForDisplay() const {
        return !initialAdminPassword_.isEmpty() && !adminInitialized();
    }

    const String& initialAdminPasswordForDisplay() const {
        return initialAdminPassword_;
    }

    bool verifyAdminPassword(const String& password) const {
        const String salt = prefs_.getString("admin_salt", "");
        const String expected = prefs_.getString("admin_hash", "");
        if (salt.isEmpty() || expected.isEmpty()) return false;
        const String actual = pbkdf2Sha256(password, salt, kIterations);
        return constantTimeEquals(actual, expected);
    }

    bool setAdminPassword(const String& password) {
        if (password.length() < 10) return false;
        const String salt = generateHex(16);
        const String hash = pbkdf2Sha256(password, salt, kIterations);
        if (hash.isEmpty()) return false;

        prefs_.putString("admin_salt", salt);
        prefs_.putString("admin_hash", hash);
        prefs_.putBool("admin_init", true);
        initialAdminPassword_ = "";
        return true;
    }

    void clear() {
        prefs_.clear();
        initialAdminPassword_ = "";
    }

private:
    static constexpr uint32_t kIterations = 120000;

    static String generateSecret(size_t length) {
        static constexpr char alphabet[] =
            "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
        String out;
        out.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            out += alphabet[esp_random() % (sizeof(alphabet) - 1)];
        }
        return out;
    }

    static String generateHex(size_t bytes) {
        static constexpr char hex[] = "0123456789abcdef";
        String out;
        out.reserve(bytes * 2);
        for (size_t i = 0; i < bytes; ++i) {
            const uint8_t value = static_cast<uint8_t>(esp_random() & 0xff);
            out += hex[value >> 4];
            out += hex[value & 0x0f];
        }
        return out;
    }

    static bool constantTimeEquals(const String& a, const String& b) {
        if (a.length() != b.length()) return false;
        uint8_t diff = 0;
        for (size_t i = 0; i < a.length(); ++i) {
            diff |= static_cast<uint8_t>(a[i] ^ b[i]);
        }
        return diff == 0;
    }

    static String pbkdf2Sha256(const String& password, const String& salt, uint32_t iterations) {
        const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        if (info == nullptr) return "";

        // PBKDF2 block 1, enough for one 32-byte SHA-256 derived key.
        uint8_t saltBlock[68] = {0};
        const size_t saltLen = salt.length();
        if (saltLen > 64) return "";
        memcpy(saltBlock, salt.c_str(), saltLen);
        saltBlock[saltLen + 3] = 1;

        uint8_t u[32] = {0};
        uint8_t t[32] = {0};
        if (mbedtls_md_hmac(info,
                            reinterpret_cast<const unsigned char*>(password.c_str()), password.length(),
                            saltBlock, saltLen + 4, u) != 0) {
            return "";
        }
        memcpy(t, u, sizeof(t));

        for (uint32_t i = 1; i < iterations; ++i) {
            uint8_t next[32] = {0};
            if (mbedtls_md_hmac(info,
                                reinterpret_cast<const unsigned char*>(password.c_str()), password.length(),
                                u, sizeof(u), next) != 0) {
                return "";
            }
            memcpy(u, next, sizeof(u));
            for (size_t j = 0; j < sizeof(t); ++j) t[j] ^= u[j];
        }

        static constexpr char hex[] = "0123456789abcdef";
        String out;
        out.reserve(64);
        for (uint8_t value : t) {
            out += hex[value >> 4];
            out += hex[value & 0x0f];
        }
        return out;
    }

    Preferences prefs_;
    String initialAdminPassword_;
};

} // namespace multibus
