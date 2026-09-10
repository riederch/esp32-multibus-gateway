#pragma once

#include <Arduino.h>
#include <vector>

namespace multibus {

class CapabilityRegistry {
public:
    bool add(const String& capability) {
        if (has(capability)) {
            return false;
        }
        capabilities_.push_back(capability);
        return true;
    }

    bool has(const String& capability) const {
        for (const auto& item : capabilities_) {
            if (item == capability) {
                return true;
            }
        }
        return false;
    }

    const std::vector<String>& all() const {
        return capabilities_;
    }

    void clear() {
        capabilities_.clear();
    }

private:
    std::vector<String> capabilities_;
};

} // namespace multibus
