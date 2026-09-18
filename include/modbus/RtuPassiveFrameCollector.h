#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace multibus {
namespace modbus {

enum class PassiveFrameResult : uint8_t {
    Idle,
    Pending,
    FrameReady,
    Overflow,
};

template <size_t Capacity>
class RtuPassiveFrameCollector {
public:
    static_assert(Capacity > 0, "Passive frame capacity must be greater than zero");

    void reset() {
        length_ = 0;
        lastByteAtUs_ = 0;
        overflow_ = false;
    }

    bool feed(uint8_t value, uint32_t nowUs) {
        lastByteAtUs_ = nowUs;
        if (overflow_) return false;
        if (length_ >= Capacity) {
            overflow_ = true;
            return false;
        }
        buffer_[length_++] = value;
        return true;
    }

    PassiveFrameResult poll(uint32_t nowUs,
                            uint32_t idleGapUs,
                            uint8_t* output,
                            size_t outputCapacity,
                            size_t& written) {
        written = 0;
        if (length_ == 0 && !overflow_) return PassiveFrameResult::Idle;
        if (lastByteAtUs_ != 0 &&
            static_cast<uint32_t>(nowUs - lastByteAtUs_) < idleGapUs) {
            return PassiveFrameResult::Pending;
        }

        if (overflow_) {
            reset();
            return PassiveFrameResult::Overflow;
        }
        if (output == nullptr || outputCapacity < length_) {
            reset();
            return PassiveFrameResult::Overflow;
        }

        memcpy(output, buffer_, length_);
        written = length_;
        reset();
        return PassiveFrameResult::FrameReady;
    }

    bool pending() const {
        return length_ != 0 || overflow_;
    }

    size_t length() const { return length_; }

private:
    uint8_t buffer_[Capacity] = {0};
    size_t length_ = 0;
    uint32_t lastByteAtUs_ = 0;
    bool overflow_ = false;
};

} // namespace modbus
} // namespace multibus
