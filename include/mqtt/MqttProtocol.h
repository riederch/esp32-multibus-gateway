#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace multibus::mqtt {

inline bool validTopicPrefix(const char* prefix) {
    if (prefix == nullptr || prefix[0] == '\0') return false;
    size_t length = 0;
    bool lastSlash = false;
    for (const char* p = prefix; *p != '\0'; ++p) {
        const char c = *p;
        if (++length > 64) return false;
        if (c == '#' || c == '+' || c == '\\' || c == ' ') return false;
        if (c == '/') {
            if (p == prefix || lastSlash) return false;
            lastSlash = true;
        } else {
            lastSlash = false;
        }
    }
    return !lastSlash;
}

inline bool validDeviceId(const char* deviceId) {
    if (deviceId == nullptr || deviceId[0] == '\0') return false;
    size_t length = 0;
    for (const char* p = deviceId; *p != '\0'; ++p) {
        const char c = *p;
        if (++length > 64) return false;
        const bool allowed =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_';
        if (!allowed) return false;
    }
    return true;
}

inline bool makeChannelStateTopic(const char* prefix,
                                  const char* deviceId,
                                  uint16_t channelId,
                                  char* output,
                                  size_t capacity) {
    if (!validTopicPrefix(prefix) || !validDeviceId(deviceId) ||
        channelId == 0 || output == nullptr || capacity == 0) {
        return false;
    }
    const int written = snprintf(
        output, capacity, "%s/%s/channels/%u/state",
        prefix, deviceId, static_cast<unsigned>(channelId));
    return written > 0 && static_cast<size_t>(written) < capacity;
}

inline bool makeChannelCommandTopic(const char* prefix,
                                    const char* deviceId,
                                    uint16_t channelId,
                                    char* output,
                                    size_t capacity) {
    if (!validTopicPrefix(prefix) || !validDeviceId(deviceId) ||
        channelId == 0 || output == nullptr || capacity == 0) {
        return false;
    }
    const int written = snprintf(
        output, capacity, "%s/%s/channels/%u/set",
        prefix, deviceId, static_cast<unsigned>(channelId));
    return written > 0 && static_cast<size_t>(written) < capacity;
}

inline bool makeChannelCommandWildcard(const char* prefix,
                                         const char* deviceId,
                                         char* output,
                                         size_t capacity) {
    if (!validTopicPrefix(prefix) || !validDeviceId(deviceId) ||
        output == nullptr || capacity == 0) {
        return false;
    }
    const int written = snprintf(
        output, capacity, "%s/%s/channels/+/set",
        prefix, deviceId);
    return written > 0 && static_cast<size_t>(written) < capacity;
}

inline bool makeAvailabilityTopic(const char* prefix,
                                  const char* deviceId,
                                  char* output,
                                  size_t capacity) {
    if (!validTopicPrefix(prefix) || !validDeviceId(deviceId) ||
        output == nullptr || capacity == 0) {
        return false;
    }
    const int written = snprintf(
        output, capacity, "%s/%s/status",
        prefix, deviceId);
    return written > 0 && static_cast<size_t>(written) < capacity;
}

struct CommandTopic {
    bool valid = false;
    uint16_t channelId = 0;
};

inline CommandTopic parseChannelCommandTopic(const char* prefix,
                                             const char* deviceId,
                                             const char* topic) {
    CommandTopic result;
    if (!validTopicPrefix(prefix) || !validDeviceId(deviceId) || topic == nullptr) return result;

    char base[160] = {0};
    const int baseLength = snprintf(base, sizeof(base), "%s/%s/channels/", prefix, deviceId);
    if (baseLength <= 0 || static_cast<size_t>(baseLength) >= sizeof(base)) return result;
    if (strncmp(topic, base, static_cast<size_t>(baseLength)) != 0) return result;

    const char* p = topic + baseLength;
    if (*p < '0' || *p > '9') return result;
    uint32_t id = 0;
    while (*p >= '0' && *p <= '9') {
        id = id * 10U + static_cast<uint32_t>(*p - '0');
        if (id > 65535U) return result;
        ++p;
    }
    if (id == 0 || strcmp(p, "/set") != 0) return result;

    result.valid = true;
    result.channelId = static_cast<uint16_t>(id);
    return result;
}

} // namespace multibus::mqtt
