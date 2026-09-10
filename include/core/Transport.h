#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

namespace multibus {

struct TransportEnvelope {
    uint8_t endpoint = 0;
    const uint8_t* payload = nullptr;
    size_t length = 0;
    bool confirmed = false;
};

class Transport {
public:
    virtual ~Transport() = default;
    virtual const char* transportId() const = 0;
    virtual bool connected() const = 0;
    virtual bool send(const TransportEnvelope& envelope) = 0;
};

} // namespace multibus
