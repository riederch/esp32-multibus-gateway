#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace multibus {

enum class EventSeverity : uint8_t {
    Info = 0,
    Warning = 1,
    Error = 2,
};

constexpr size_t kEventSourceLength = 32;
constexpr size_t kEventTypeLength = 32;
constexpr size_t kEventDetailLength = 128;

struct EventRecord {
    uint64_t sequence = 0;
    uint32_t timestampUnix = 0;
    EventSeverity severity = EventSeverity::Info;
    char source[kEventSourceLength] = {0};
    char type[kEventTypeLength] = {0};
    char detail[kEventDetailLength] = {0};
};

inline bool setEventText(char* output,
                         size_t capacity,
                         const char* input) {
    if (output == nullptr || capacity == 0 || input == nullptr) return false;
    const size_t length = strnlen(input, capacity);
    if (length == 0 || length >= capacity) return false;
    memset(output, 0, capacity);
    memcpy(output, input, length);
    return true;
}

template <size_t Capacity>
class EventBus {
public:
    static_assert(Capacity > 0, "EventBus capacity must be greater than zero");

    bool emit(const char* source,
              const char* type,
              EventSeverity severity,
              uint32_t timestampUnix,
              const char* detail = "") {
        EventRecord record;
        if (!setEventText(record.source, sizeof(record.source), source) ||
            !setEventText(record.type, sizeof(record.type), type)) {
            return false;
        }

        if (detail != nullptr && detail[0] != '\0') {
            if (!setEventText(record.detail, sizeof(record.detail), detail)) return false;
        }

        record.sequence = ++latestSequence_;
        record.timestampUnix = timestampUnix;
        record.severity = severity;

        records_[(record.sequence - 1U) % Capacity] = record;
        if (count_ < Capacity) ++count_;
        return true;
    }

    void clear() {
        for (auto& record : records_) record = EventRecord{};
        count_ = 0;
        latestSequence_ = 0;
    }

    size_t size() const { return count_; }
    constexpr size_t capacity() const { return Capacity; }
    uint64_t latestSequence() const { return latestSequence_; }

    uint64_t oldestSequence() const {
        if (count_ == 0) return 0;
        return latestSequence_ - static_cast<uint64_t>(count_) + 1U;
    }

    uint64_t missedSince(uint64_t cursor) const {
        if (count_ == 0) return 0;
        const uint64_t oldest = oldestSequence();
        const uint64_t wanted = cursor + 1U;
        return wanted < oldest ? oldest - wanted : 0;
    }

    bool read(uint64_t sequence, EventRecord& record) const {
        const uint64_t oldest = oldestSequence();
        if (sequence == 0 || oldest == 0 ||
            sequence < oldest || sequence > latestSequence_) {
            return false;
        }

        const EventRecord& stored = records_[(sequence - 1U) % Capacity];
        if (stored.sequence != sequence) return false;
        record = stored;
        return true;
    }

    bool next(uint64_t& cursor, EventRecord& record) const {
        if (count_ == 0) return false;

        const uint64_t oldest = oldestSequence();
        uint64_t target = cursor + 1U;
        if (target < oldest) target = oldest;
        if (target > latestSequence_) return false;

        if (!read(target, record)) return false;
        cursor = target;
        return true;
    }

private:
    EventRecord records_[Capacity];
    size_t count_ = 0;
    uint64_t latestSequence_ = 0;
};

} // namespace multibus
