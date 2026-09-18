#pragma once

#include <stdint.h>

namespace multibus::history {

struct RetransmissionState {
    bool pending = false;
    uint32_t lostAtUnix = 0;
};

} // namespace multibus::history
