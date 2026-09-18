#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace multibus::modbus {

enum class PassiveFrameResult : uint8_t {
    Idle,
    Pending,
    Ready,
    Overflow,
};

template <size_t Capacity>
class PassiveFrameAssembler {
public:
    static_assert(Capacity > 0, "PassiveFrameAssembler capacity must be positive");

    void clear() {
        length_ = 0;
        lastByteAtUs_ = 0;
        overflow_ = false;
    }

    bool append(uint8_t value, uint32_t nowUs) {
        if (length_ >= Capacity) {
            overflow_ = true;
            return false;
        }
        data_[length_++] = value;
        lastByteAtUs_ = nowUs;
        return true;
    }

    PassiveFrameResult poll(uint32_t nowUs,
                            uint32_t idleGapUs,
                            uint8_t* output,
                            size_t capacity,
                            size_t& written) {
        written = 0;
        if (overflow_) {
            clear();
            return PassiveFrameResult::Overflow;
        }
        if (length_ == 0) return PassiveFrameResult::Idle;
        if (lastByteAtUs_ == 0 ||
            static_cast<uint32_t>(nowUs - lastByteAtUs_) < idleGapUs) {
            return PassiveFrameResult::Pending;
        }
        if (output == nullptr || capacity < length_) {
            clear();
            return PassiveFrameResult::Overflow;
        }

        memcpy(output, data_, length_);
        written = length_;
        clear();
        return PassiveFrameResult::Ready;
    }

    bool pending() const { return length_ != 0; }
    size_t size() const { return length_; }

private:
    uint8_t data_[Capacity] = {0};
    size_t length_ = 0;
    uint32_t lastByteAtUs_ = 0;
    bool overflow_ = false;
};

} // namespace multibus::modbus
