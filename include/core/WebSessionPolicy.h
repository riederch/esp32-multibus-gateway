#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace multibus {

struct WebSessionPolicy {
    static constexpr uint32_t kIdleTimeoutMs = 30UL * 60UL * 1000UL;
    static constexpr uint32_t kMaxLifetimeMs = 12UL * 60UL * 60UL * 1000UL;

    static bool expired(uint32_t createdAt,
                        uint32_t lastActivityAt,
                        uint32_t now) {
        return static_cast<uint32_t>(now - createdAt) > kMaxLifetimeMs ||
               static_cast<uint32_t>(now - lastActivityAt) > kIdleTimeoutMs;
    }

    static bool hasSessionCookie(const char* cookieHeader,
                                 const char* expectedToken) {
        if (cookieHeader == nullptr || expectedToken == nullptr ||
            expectedToken[0] == '\0') {
            return false;
        }

        static constexpr char kName[] = "MBSESSION";
        const size_t expectedLength = std::strlen(expectedToken);
        const char* cursor = cookieHeader;

        while (*cursor != '\0') {
            while (*cursor == ';' || *cursor == ' ' || *cursor == '\t') ++cursor;
            if (*cursor == '\0') break;

            const char* nameStart = cursor;
            while (*cursor != '\0' && *cursor != '=' && *cursor != ';') ++cursor;
            const char* nameEnd = cursor;
            while (nameEnd > nameStart &&
                   (nameEnd[-1] == ' ' || nameEnd[-1] == '\t')) {
                --nameEnd;
            }

            if (*cursor != '=') {
                while (*cursor != '\0' && *cursor != ';') ++cursor;
                continue;
            }

            ++cursor;
            while (*cursor == ' ' || *cursor == '\t') ++cursor;
            const char* valueStart = cursor;
            while (*cursor != '\0' && *cursor != ';') ++cursor;
            const char* valueEnd = cursor;
            while (valueEnd > valueStart &&
                   (valueEnd[-1] == ' ' || valueEnd[-1] == '\t')) {
                --valueEnd;
            }

            const size_t nameLength = static_cast<size_t>(nameEnd - nameStart);
            const size_t valueLength = static_cast<size_t>(valueEnd - valueStart);
            if (nameLength == sizeof(kName) - 1 &&
                std::memcmp(nameStart, kName, sizeof(kName) - 1) == 0 &&
                valueLength == expectedLength &&
                constantTimeEquals(valueStart, expectedToken, expectedLength)) {
                return true;
            }
        }

        return false;
    }

    static bool csrfMatches(const char* supplied, const char* expected) {
        if (supplied == nullptr || expected == nullptr) return false;
        const size_t suppliedLength = std::strlen(supplied);
        const size_t expectedLength = std::strlen(expected);
        if (suppliedLength == 0 || suppliedLength != expectedLength) return false;
        return constantTimeEquals(supplied, expected, expectedLength);
    }

private:
    static bool constantTimeEquals(const char* a,
                                   const char* b,
                                   size_t length) {
        uint8_t diff = 0;
        for (size_t i = 0; i < length; ++i) {
            diff |= static_cast<uint8_t>(
                static_cast<unsigned char>(a[i]) ^
                static_cast<unsigned char>(b[i]));
        }
        return diff == 0;
    }
};

} // namespace multibus
