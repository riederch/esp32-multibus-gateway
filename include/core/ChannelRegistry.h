#pragma once

#include <Arduino.h>
#include <vector>
#include "DataSource.h"

namespace multibus {

class ChannelRegistry {
public:
    static constexpr size_t kRecommendedMaxChannels = 64;
    static constexpr uint8_t kCompatibilitySlots = 32;

    bool upsert(const ChannelBinding& binding, String* error = nullptr) {
        if (binding.channelId == 0) {
            if (error) *error = "channel-id-zero";
            return false;
        }
        if (binding.sourceId.isEmpty() || binding.pointId.isEmpty()) {
            if (error) *error = "missing-source-binding";
            return false;
        }

        for (auto& current : channels_) {
            if (current.channelId == binding.channelId) {
                current = binding;
                return true;
            }
        }

        channels_.push_back(binding);
        return true;
    }

    bool remove(uint16_t channelId) {
        for (auto it = channels_.begin(); it != channels_.end(); ++it) {
            if (it->channelId == channelId) {
                channels_.erase(it);
                return true;
            }
        }
        return false;
    }

    ChannelBinding* find(uint16_t channelId) {
        for (auto& channel : channels_) {
            if (channel.channelId == channelId) return &channel;
        }
        return nullptr;
    }

    const ChannelBinding* find(uint16_t channelId) const {
        for (const auto& channel : channels_) {
            if (channel.channelId == channelId) return &channel;
        }
        return nullptr;
    }

    size_t size() const { return channels_.size(); }
    bool empty() const { return channels_.empty(); }
    void clear() { channels_.clear(); }

    const std::vector<ChannelBinding>& all() const { return channels_; }

    static bool compatibilityDownlinkIdToSlot(uint8_t channelId, uint8_t& slot) {
        if (channelId < 1 || channelId > kCompatibilitySlots) return false;
        slot = static_cast<uint8_t>(channelId - 1);
        return true;
    }

    static bool compatibilityUplinkSlotValid(uint8_t slot) {
        return slot < kCompatibilitySlots;
    }

private:
    std::vector<ChannelBinding> channels_;
};

} // namespace multibus
