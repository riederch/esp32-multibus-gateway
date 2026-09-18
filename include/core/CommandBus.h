#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace multibus {

enum class CommandValueType : uint8_t {
    Boolean = 0,
    Int64 = 1,
    UInt64 = 2,
    Float64 = 3,
    Text = 4,
};

constexpr size_t kCommandOriginLength = 16;
constexpr size_t kCommandTextLength = 96;

struct ChannelWriteCommand {
    uint64_t sequence = 0;
    uint16_t channelId = 0;
    CommandValueType valueType = CommandValueType::Float64;
    bool booleanValue = false;
    int64_t intValue = 0;
    uint64_t uintValue = 0;
    double floatValue = 0.0;
    char textValue[kCommandTextLength] = {0};
    char origin[kCommandOriginLength] = {0};
};

inline bool setCommandText(char* output,
                           size_t capacity,
                           const char* input,
                           bool allowEmpty = false) {
    if (output == nullptr || capacity == 0 || input == nullptr) return false;
    const size_t length = strnlen(input, capacity);
    if ((!allowEmpty && length == 0) || length >= capacity) return false;
    memset(output, 0, capacity);
    if (length > 0) memcpy(output, input, length);
    return true;
}

template <size_t Capacity>
class CommandBus {
public:
    static_assert(Capacity > 0, "CommandBus capacity must be greater than zero");

    bool enqueue(ChannelWriteCommand command) {
        if (count_ >= Capacity || command.channelId == 0 ||
            !setCommandText(command.origin, sizeof(command.origin), command.origin)) {
            return false;
        }

        command.sequence = ++latestSequence_;
        queue_[tail_] = command;
        tail_ = (tail_ + 1U) % Capacity;
        ++count_;
        return true;
    }

    bool dequeue(ChannelWriteCommand& command) {
        if (count_ == 0) return false;
        command = queue_[head_];
        queue_[head_] = ChannelWriteCommand{};
        head_ = (head_ + 1U) % Capacity;
        --count_;
        return true;
    }

    size_t size() const { return count_; }
    constexpr size_t capacity() const { return Capacity; }
    bool empty() const { return count_ == 0; }
    bool full() const { return count_ == Capacity; }
    uint64_t latestSequence() const { return latestSequence_; }

    void clear() {
        for (auto& command : queue_) command = ChannelWriteCommand{};
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }

private:
    ChannelWriteCommand queue_[Capacity];
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    uint64_t latestSequence_ = 0;
};

} // namespace multibus
