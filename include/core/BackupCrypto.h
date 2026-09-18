#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <mbedtls/base64.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <stdint.h>
#include <string.h>
#include <vector>

namespace multibus {

class BackupCrypto {
public:
    static constexpr uint32_t kIterations = 150000;
    static constexpr size_t kSaltLength = 16;
    static constexpr size_t kNonceLength = 12;
    static constexpr size_t kTagLength = 16;
    static constexpr size_t kKeyLength = 32;
    static constexpr size_t kMinimumPassphraseLength = 12;

    static bool encrypt(const String& plaintext,
                        const String& passphrase,
                        String& output,
                        String& error) {
        if (plaintext.isEmpty()) {
            error = "empty-backup";
            return false;
        }
        if (passphrase.length() < kMinimumPassphraseLength) {
            error = "passphrase-too-short";
            return false;
        }

        uint8_t salt[kSaltLength] = {0};
        uint8_t nonce[kNonceLength] = {0};
        esp_fill_random(salt, sizeof(salt));
        esp_fill_random(nonce, sizeof(nonce));

        uint8_t key[kKeyLength] = {0};
        if (!deriveKey(passphrase, salt, sizeof(salt), kIterations, key)) {
            error = "key-derivation-failed";
            return false;
        }

        std::vector<uint8_t> ciphertext(plaintext.length());
        uint8_t tag[kTagLength] = {0};

        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);
        const int setKey = mbedtls_gcm_setkey(
            &gcm, MBEDTLS_CIPHER_ID_AES, key, kKeyLength * 8U);
        const int encrypted = setKey == 0
            ? mbedtls_gcm_crypt_and_tag(
                &gcm,
                MBEDTLS_GCM_ENCRYPT,
                plaintext.length(),
                nonce,
                sizeof(nonce),
                reinterpret_cast<const uint8_t*>(kAad),
                strlen(kAad),
                reinterpret_cast<const uint8_t*>(plaintext.c_str()),
                ciphertext.data(),
                sizeof(tag),
                tag)
            : setKey;
        mbedtls_gcm_free(&gcm);
        secureZero(key, sizeof(key));

        if (encrypted != 0) {
            error = "encryption-failed";
            return false;
        }

        String salt64;
        String nonce64;
        String tag64;
        String ciphertext64;
        if (!encodeBase64(salt, sizeof(salt), salt64) ||
            !encodeBase64(nonce, sizeof(nonce), nonce64) ||
            !encodeBase64(tag, sizeof(tag), tag64) ||
            !encodeBase64(ciphertext.data(), ciphertext.size(), ciphertext64)) {
            error = "base64-encode-failed";
            return false;
        }

        JsonDocument doc;
        doc["format"] = "multibus-backup-encrypted";
        doc["version"] = 1;
        doc["kdf"] = "pbkdf2-hmac-sha256";
        doc["iterations"] = kIterations;
        doc["cipher"] = "aes-256-gcm";
        doc["salt"] = salt64;
        doc["nonce"] = nonce64;
        doc["tag"] = tag64;
        doc["ciphertext"] = ciphertext64;

        output = "";
        serializeJsonPretty(doc, output);
        error = "";
        return !output.isEmpty();
    }

    static bool decrypt(const String& input,
                        const String& passphrase,
                        String& plaintext,
                        String& error) {
        if (passphrase.length() < kMinimumPassphraseLength) {
            error = "passphrase-too-short";
            return false;
        }

        JsonDocument doc;
        const DeserializationError parseError = deserializeJson(doc, input);
        if (parseError) {
            error = "invalid-encrypted-json";
            return false;
        }
        if (String(doc["format"] | "") != "multibus-backup-encrypted" ||
            (doc["version"] | 0U) != 1U ||
            String(doc["kdf"] | "") != "pbkdf2-hmac-sha256" ||
            String(doc["cipher"] | "") != "aes-256-gcm") {
            error = "unsupported-encrypted-backup";
            return false;
        }

        const uint32_t iterations = doc["iterations"] | 0U;
        if (iterations < 100000U || iterations > 1000000U) {
            error = "invalid-kdf-iterations";
            return false;
        }

        std::vector<uint8_t> salt;
        std::vector<uint8_t> nonce;
        std::vector<uint8_t> tag;
        std::vector<uint8_t> ciphertext;
        if (!decodeBase64(String(doc["salt"] | ""), salt) ||
            !decodeBase64(String(doc["nonce"] | ""), nonce) ||
            !decodeBase64(String(doc["tag"] | ""), tag) ||
            !decodeBase64(String(doc["ciphertext"] | ""), ciphertext) ||
            salt.size() != kSaltLength ||
            nonce.size() != kNonceLength ||
            tag.size() != kTagLength ||
            ciphertext.empty()) {
            error = "invalid-encrypted-fields";
            return false;
        }

        uint8_t key[kKeyLength] = {0};
        if (!deriveKey(passphrase, salt.data(), salt.size(), iterations, key)) {
            error = "key-derivation-failed";
            return false;
        }

        std::vector<uint8_t> decoded(ciphertext.size() + 1U, 0);
        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);
        const int setKey = mbedtls_gcm_setkey(
            &gcm, MBEDTLS_CIPHER_ID_AES, key, kKeyLength * 8U);
        const int decrypted = setKey == 0
            ? mbedtls_gcm_auth_decrypt(
                &gcm,
                ciphertext.size(),
                nonce.data(),
                nonce.size(),
                reinterpret_cast<const uint8_t*>(kAad),
                strlen(kAad),
                tag.data(),
                tag.size(),
                ciphertext.data(),
                decoded.data())
            : setKey;
        mbedtls_gcm_free(&gcm);
        secureZero(key, sizeof(key));

        if (decrypted != 0) {
            error = "authentication-failed";
            return false;
        }

        plaintext = String(
            reinterpret_cast<const char*>(decoded.data()),
            ciphertext.size());
        error = "";
        return !plaintext.isEmpty();
    }

    static bool isEncryptedEnvelope(const String& input) {
        return input.indexOf(""format"") >= 0 &&
               input.indexOf("multibus-backup-encrypted") >= 0;
    }

private:
    static constexpr const char* kAad = "esp32-multibus-gateway:backup:v1";

    static void secureZero(void* ptr, size_t length) {
        volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
        while (length-- > 0) *p++ = 0;
    }

    static bool deriveKey(const String& passphrase,
                          const uint8_t* salt,
                          size_t saltLength,
                          uint32_t iterations,
                          uint8_t output[kKeyLength]) {
        if (salt == nullptr || saltLength == 0 || iterations == 0) return false;
        const mbedtls_md_info_t* info =
            mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        if (info == nullptr) return false;

        std::vector<uint8_t> saltBlock(saltLength + 4U, 0);
        memcpy(saltBlock.data(), salt, saltLength);
        saltBlock[saltLength + 3U] = 1U;

        uint8_t u[kKeyLength] = {0};
        uint8_t t[kKeyLength] = {0};
        if (mbedtls_md_hmac(
                info,
                reinterpret_cast<const uint8_t*>(passphrase.c_str()),
                passphrase.length(),
                saltBlock.data(),
                saltBlock.size(),
                u) != 0) {
            return false;
        }
        memcpy(t, u, sizeof(t));

        for (uint32_t i = 1; i < iterations; ++i) {
            uint8_t next[kKeyLength] = {0};
            if (mbedtls_md_hmac(
                    info,
                    reinterpret_cast<const uint8_t*>(passphrase.c_str()),
                    passphrase.length(),
                    u,
                    sizeof(u),
                    next) != 0) {
                secureZero(u, sizeof(u));
                secureZero(t, sizeof(t));
                return false;
            }
            memcpy(u, next, sizeof(u));
            for (size_t j = 0; j < sizeof(t); ++j) t[j] ^= u[j];
        }

        memcpy(output, t, kKeyLength);
        secureZero(u, sizeof(u));
        secureZero(t, sizeof(t));
        return true;
    }

    static bool encodeBase64(const uint8_t* input,
                             size_t length,
                             String& output) {
        if (input == nullptr || length == 0) return false;
        const size_t capacity = ((length + 2U) / 3U) * 4U + 1U;
        std::vector<uint8_t> encoded(capacity, 0);
        size_t written = 0;
        if (mbedtls_base64_encode(
                encoded.data(),
                encoded.size(),
                &written,
                input,
                length) != 0) {
            return false;
        }
        output = String(
            reinterpret_cast<const char*>(encoded.data()),
            written);
        return true;
    }

    static bool decodeBase64(const String& input,
                             std::vector<uint8_t>& output) {
        if (input.isEmpty()) return false;
        const size_t capacity = (input.length() / 4U) * 3U + 3U;
        output.assign(capacity, 0);
        size_t written = 0;
        if (mbedtls_base64_decode(
                output.data(),
                output.size(),
                &written,
                reinterpret_cast<const uint8_t*>(input.c_str()),
                input.length()) != 0) {
            output.clear();
            return false;
        }
        output.resize(written);
        return written > 0;
    }
};

} // namespace multibus
