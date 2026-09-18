#pragma once

#include <Preferences.h>

#include "HistoryImageCodec.h"

namespace multibus::history {

class PersistentHistoryStore {
public:
    using Ring = lorawan::HistoryRing<kPersistentHistoryCapacity>;
    using Codec = HistoryImageCodec<kPersistentHistoryCapacity>;

    bool begin() {
        return prefs_.begin("mbhistdata", false);
    }

    void end() {
        prefs_.end();
    }

    bool load(Ring& ring) {
        Ring a;
        Ring b;
        uint32_t generationA = 0;
        uint32_t generationB = 0;
        const bool validA = loadKey(kSlotA, a, generationA);
        const bool validB = loadKey(kSlotB, b, generationB);

        if (!validA && !validB) {
            ring.clear();
            generation_ = 0;
            return !prefs_.isKey(kSlotA) && !prefs_.isKey(kSlotB);
        }

        if (validA && (!validB || newerGeneration(generationA, generationB))) {
            ring = a;
            generation_ = generationA;
        } else {
            ring = b;
            generation_ = generationB;
        }
        return true;
    }

    bool save(const Ring& ring) {
        uint8_t image[Codec::kImageLength] = {0};
        size_t written = 0;
        const uint32_t nextGeneration = generation_ + 1U;
        if (!Codec::encode(ring, nextGeneration, image, sizeof(image), written) ||
            written != sizeof(image)) {
            return false;
        }

        const char* key = (nextGeneration & 1U) == 0 ? kSlotA : kSlotB;
        if (prefs_.putBytes(key, image, sizeof(image)) != sizeof(image)) return false;
        generation_ = nextGeneration;
        return true;
    }

    void clear() {
        prefs_.clear();
        generation_ = 0;
    }

    uint32_t generation() const { return generation_; }

private:
    bool loadKey(const char* key, Ring& ring, uint32_t& generation) const {
        const size_t length = prefs_.getBytesLength(key);
        if (length == 0) return false;
        if (length != Codec::kImageLength) return false;

        uint8_t image[Codec::kImageLength] = {0};
        if (prefs_.getBytes(key, image, sizeof(image)) != sizeof(image)) return false;
        return Codec::decode(image, sizeof(image), ring, generation);
    }

    static bool newerGeneration(uint32_t a, uint32_t b) {
        return static_cast<int32_t>(a - b) > 0;
    }

    static constexpr const char* kSlotA = "ring_a";
    static constexpr const char* kSlotB = "ring_b";

    mutable Preferences prefs_;
    uint32_t generation_ = 0;
};

} // namespace multibus::history
