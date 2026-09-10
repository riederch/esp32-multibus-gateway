#pragma once

namespace multibus {

class CapabilityRegistry;

class Component {
public:
    virtual ~Component() = default;

    virtual const char* name() const = 0;
    virtual bool begin(CapabilityRegistry& capabilities) = 0;
    virtual void loop() = 0;
    virtual void end() {}
};

} // namespace multibus
