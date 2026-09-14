#pragma once

#include <Arduino.h>
#include <vector>
#include "core/AppConfig.h"
#include "core/CapabilityRegistry.h"
#include "core/Component.h"
#include "core/DataSource.h"
#include "core/DeviceConfig.h"
#include "core/LoRaWanIdentity.h"
#include "core/Transport.h"
#include "lorawan/LoRaWanRadio.h"
#include "modbus/ModbusChannel.h"
#include "modbus/ModbusMasterSettings.h"
#include "modbus/ModbusRtuMaster.h"

namespace multibus {

class VictronComponent final : public Component, public DataSource {
public:
    void setMode(VictronMode mode) { mode_ = mode; active_ = false; }
    const char* name() const override { return "victron"; }
    const char* sourceId() const override { return "victron:vebus"; }

    bool begin(CapabilityRegistry& capabilities) override {
        if (mode_ != VictronMode::Enabled) return true;
        capabilities.add("victron.telemetry");
        capabilities.add("victron.settings");
        capabilities.add("victron.usb.mk3");
        capabilities.add("victron.ble.victronconnect");
        capabilities.add("source.victron");
        active_ = true;
        return true;
    }

    void loop() override {}
    bool active() const { return active_; }
    bool online() const override { return false; }
    size_t pointCount() const override { return 0; }
    bool describePoint(size_t, DataPointDescriptor&) const override { return false; }
    bool readPoint(const String&, DataValue&) override { return false; }
    bool writePoint(const String&, const DataValue&) override { return false; }

private:
    VictronMode mode_ = VictronMode::Disabled;
    bool active_ = false;
};

class LoRaComponent final : public Component, public Transport {
public:
    using DownlinkHandler = lorawan::LoRaWanRadio::DownlinkHandler;

    void setMode(LoRaMode mode) { mode_ = mode; active_ = false; }

    void setProvisioning(const LoRaWanConfig& config, const String& devEuiValue) {
        lorawan_ = config;
        devEui_ = devEuiValue;
        radio_.configure(lorawan_, devEui_);
    }

    void setDownlinkHandler(DownlinkHandler handler, void* context) {
        radio_.setDownlinkHandler(handler, context);
    }

    const char* name() const override { return "lora"; }
    const char* transportId() const override { return "lora"; }

    bool begin(CapabilityRegistry& capabilities) override {
        if (mode_ == LoRaMode::Disabled) return true;
        if (mode_ == LoRaMode::Meshtastic) return false;

        capabilities.add("transport.lora");
        capabilities.add("lora.lorawan");
        capabilities.add("lora.uplink");
        capabilities.add("lora.downlink");
        capabilities.add("lora.compat.fport85");
        capabilities.add("lora.extensions");
        capabilities.add("lora.otaa");
        capabilities.add("lora.eu868");
        capabilities.add("lora.sx1262");

        provisioned_ = radio_.provisioned();
        if (!provisioned_) return false;
        active_ = radio_.begin();
        return active_;
    }

    void loop() override { radio_.loop(); }
    bool active() const { return active_; }
    bool provisioned() const { return provisioned_; }
    bool connected() const override { return active_ && radio_.joined(); }
    bool send(const TransportEnvelope& envelope) override {
        return active_ && radio_.send(envelope);
    }

    const String& devEuiValue() const { return devEui_; }
    const LoRaWanConfig& provisioning() const { return lorawan_; }
    int16_t lastState() const { return radio_.lastState(); }
    uint32_t devAddr() const { return radio_.devAddr(); }

private:
    LoRaMode mode_ = LoRaMode::Disabled;
    LoRaWanConfig lorawan_;
    String devEui_;
    lorawan::LoRaWanRadio radio_;
    bool active_ = false;
    bool provisioned_ = false;
};

class ModbusComponent final : public Component, public DataSource {
public:
    struct PollCompletion {
        modbus::ChannelConfig channel;
        bool success = false;
        uint8_t valueCount = 0;
        modbus::DecodedScalar values[2];
        modbus::RtuDecodeStatus status = modbus::RtuDecodeStatus::Truncated;
        uint8_t exceptionCode = 0;
        uint32_t completedAtMs = 0;
    };

    void setMode(ModbusMode mode) {
        mode_ = mode;
        active_ = false;
    }

    void setRtuSerialConfig(const modbus::RtuSerialConfig& config) {
        serialConfig_ = config;
    }

    bool applyRs485SerialSettings(const modbus::Rs485SerialSettings& settings) {
        modbus::RtuSerialConfig config;
        if (!modbus::makeRtuSerialConfig(settings, config, masterSettings_.maxResponseTimeMs)) return false;

        if (mode_ == ModbusMode::Master && active_) {
            if (!rtuMaster_.reconfigure(config)) return false;
        }

        rs485Settings_ = settings;
        serialConfig_ = config;
        return true;
    }

    const modbus::Rs485SerialSettings& rs485SerialSettings() const {
        return rs485Settings_;
    }

    bool applyModbusMasterSettings(const modbus::ModbusMasterSettings& settings) {
        if (!modbus::runtimeSupportsModbusMasterSettings(settings)) return false;

        modbus::RtuSerialConfig config;
        if (!modbus::makeRtuSerialConfig(rs485Settings_, config, settings.maxResponseTimeMs)) return false;

        if (mode_ == ModbusMode::Master && active_) {
            if (!rtuMaster_.reconfigure(config)) return false;
        }

        masterSettings_ = settings;
        serialConfig_ = config;
        pollRetryCount_ = 0;
        nextPollAtMs_ = millis();
        return true;
    }

    const modbus::ModbusMasterSettings& modbusMasterSettings() const {
        return masterSettings_;
    }

    const char* name() const override { return "modbus"; }
    const char* sourceId() const override { return "modbus"; }

    bool begin(CapabilityRegistry& capabilities) override {
        switch (mode_) {
            case ModbusMode::Master:
                capabilities.add("source.modbus");
                capabilities.add("modbus.master");
                capabilities.add("modbus.rtu");
                capabilities.add("modbus.read");
                capabilities.add("modbus.write");
                capabilities.add("modbus.raw");
                capabilities.add("modbus.polling");
                capabilities.add("modbus.compatibility-channels");
                if (!rtuMaster_.begin(serialConfig_)) return false;
                active_ = true;
                nextPollAtMs_ = millis();
                return true;
            case ModbusMode::Slave:
                capabilities.add("modbus.slave");
                capabilities.add("modbus.virtual-registers");
                active_ = true;
                return true;
            case ModbusMode::Disabled:
            default:
                return true;
        }
    }

    bool upsertChannel(const modbus::ChannelConfig& config) {
        if (!modbus::validChannelConfig(config)) return false;
        cache_[config.slot] = PollCache{};
        for (auto& current : channels_) {
            if (current.slot == config.slot) {
                current = config;
                return true;
            }
        }
        channels_.push_back(config);
        return true;
    }

    bool removeChannel(uint8_t slot) {
        for (auto it = channels_.begin(); it != channels_.end(); ++it) {
            if (it->slot == slot) {
                channels_.erase(it);
                cache_[slot] = PollCache{};
                if (pollCompletionPending_ && completedPoll_.channel.slot == slot) pollCompletionPending_ = false;
                if (pollChannelIndex_ >= channels_.size()) pollChannelIndex_ = 0;
                pollRetryCount_ = 0;
                return true;
            }
        }
        return false;
    }

    bool setChannelName(uint8_t slot, const char* name, size_t length) {
        modbus::ChannelConfig* config = channelForSlot(slot);
        return config != nullptr && modbus::setChannelName(*config, name, length);
    }

    modbus::ChannelConfig* channelForSlot(uint8_t slot) {
        for (auto& channel : channels_) {
            if (channel.slot == slot) return &channel;
        }
        return nullptr;
    }

    const modbus::ChannelConfig* channelForSlot(uint8_t slot) const {
        for (const auto& channel : channels_) {
            if (channel.slot == slot) return &channel;
        }
        return nullptr;
    }

    static String pointIdForSlot(uint8_t slot, uint8_t registerIndex = 0) {
        char value[20];
        if (registerIndex == 0) {
            snprintf(value, sizeof(value), "compat/%u", static_cast<unsigned>(slot + 1));
        } else {
            snprintf(value, sizeof(value), "compat/%u/%u",
                     static_cast<unsigned>(slot + 1),
                     static_cast<unsigned>(registerIndex + 1));
        }
        return String(value);
    }

    void loop() override {
        if (mode_ != ModbusMode::Master || !active_ || channels_.empty()) return;

        const uint32_t now = millis();
        if (static_cast<int32_t>(now - nextPollAtMs_) < 0) return;
        if (pollChannelIndex_ >= channels_.size()) pollChannelIndex_ = 0;

        const modbus::ChannelConfig& channel = channels_[pollChannelIndex_];
        modbus::DecodedScalar values[2];
        uint8_t valueCount = 0;
        PollCache& cached = cache_[channel.slot];

        if (rtuMaster_.read(channel, values, valueCount)) {
            cached.valid = true;
            cached.valueCount = valueCount;
            cached.values[0] = values[0];
            if (valueCount > 1) cached.values[1] = values[1];
            cached.updatedAtMs = millis();
            cached.lastStatus = modbus::RtuDecodeStatus::Ok;
            cached.lastExceptionCode = 0;
            publishPollCompletion(channel, true, values, valueCount);
            pollRetryCount_ = 0;
            advancePollChannel();
        } else if (pollRetryCount_ < masterSettings_.maxRetryTimes) {
            ++pollRetryCount_;
            cached.lastStatus = rtuMaster_.lastStatus();
            cached.lastExceptionCode = rtuMaster_.lastExceptionCode();
        } else {
            cached.valid = false;
            cached.valueCount = 0;
            cached.lastStatus = rtuMaster_.lastStatus();
            cached.lastExceptionCode = rtuMaster_.lastExceptionCode();
            publishPollCompletion(channel, false, nullptr, 0);
            pollRetryCount_ = 0;
            advancePollChannel();
        }

        nextPollAtMs_ = millis() + masterSettings_.executionIntervalMs;
    }

    bool takeCompletedPoll(PollCompletion& completion) {
        if (!pollCompletionPending_) return false;
        completion = completedPoll_;
        pollCompletionPending_ = false;
        return true;
    }

    bool active() const { return active_; }
    bool online() const override {
        return mode_ == ModbusMode::Master && active_ && rtuMaster_.online();
    }

    size_t pointCount() const override {
        size_t count = 0;
        for (const auto& channel : channels_) count += channel.quantity;
        return count;
    }

    bool describePoint(size_t index, DataPointDescriptor& descriptor) const override {
        size_t currentIndex = 0;
        for (const auto& channel : channels_) {
            for (uint8_t registerIndex = 0; registerIndex < channel.quantity; ++registerIndex) {
                if (currentIndex++ != index) continue;

                descriptor.id = pointIdForSlot(channel.slot, registerIndex);
                descriptor.unit = "";
                descriptor.readable = mode_ == ModbusMode::Master;
                descriptor.writable = false;

                if (modbus::isBooleanType(channel.dataType)) {
                    descriptor.type = DataType::Boolean;
                } else if (modbus::isFloatingPointType(channel.dataType)) {
                    descriptor.type = DataType::Float64;
                } else if (modbus::uplinkSigned(channel)) {
                    descriptor.type = DataType::Int64;
                } else {
                    descriptor.type = DataType::UInt64;
                }
                return true;
            }
        }
        return false;
    }

    bool readPoint(const String& pointId, DataValue& value) override {
        value = DataValue{};
        if (mode_ != ModbusMode::Master || !active_) return false;

        uint8_t slot = 0;
        uint8_t registerIndex = 0;
        if (!parsePointId(pointId, slot, registerIndex)) return false;

        const modbus::ChannelConfig* channel = channelForSlot(slot);
        if (channel == nullptr || registerIndex >= channel->quantity) return false;

        const PollCache& cached = cache_[slot];
        if (!cached.valid || registerIndex >= cached.valueCount) return false;

        value.valid = true;
        const auto& scalar = cached.values[registerIndex];
        switch (scalar.kind) {
            case modbus::ScalarKind::Boolean:
                value.type = DataType::Boolean;
                value.booleanValue = scalar.booleanValue;
                return true;
            case modbus::ScalarKind::SignedInteger:
                value.type = DataType::Int64;
                value.intValue = scalar.signedValue;
                return true;
            case modbus::ScalarKind::UnsignedInteger:
                value.type = DataType::UInt64;
                value.uintValue = scalar.unsignedValue;
                return true;
            case modbus::ScalarKind::FloatingPoint:
                value.type = DataType::Float64;
                value.floatValue = scalar.floatingValue;
                return true;
        }
        value.valid = false;
        return false;
    }

    bool writePoint(const String&, const DataValue&) override { return false; }

    bool hasValidCache(uint8_t slot) const {
        return slot < modbus::kCompatibilitySlotCount && cache_[slot].valid;
    }

    uint32_t cacheAgeMs(uint8_t slot) const {
        if (!hasValidCache(slot)) return 0;
        return millis() - cache_[slot].updatedAtMs;
    }

    const modbus::ModbusRtuMaster& rtuMaster() const { return rtuMaster_; }

private:
    struct PollCache {
        bool valid = false;
        uint8_t valueCount = 0;
        modbus::DecodedScalar values[2];
        uint32_t updatedAtMs = 0;
        modbus::RtuDecodeStatus lastStatus = modbus::RtuDecodeStatus::Truncated;
        uint8_t lastExceptionCode = 0;
    };

    void publishPollCompletion(const modbus::ChannelConfig& channel,
                               bool success,
                               const modbus::DecodedScalar* values,
                               uint8_t valueCount) {
        completedPoll_ = PollCompletion{};
        completedPoll_.channel = channel;
        completedPoll_.success = success;
        completedPoll_.valueCount = success ? valueCount : 0;
        if (success && values != nullptr && valueCount > 0) {
            completedPoll_.values[0] = values[0];
            if (valueCount > 1) completedPoll_.values[1] = values[1];
        }
        completedPoll_.status = success ? modbus::RtuDecodeStatus::Ok : rtuMaster_.lastStatus();
        completedPoll_.exceptionCode = success ? 0 : rtuMaster_.lastExceptionCode();
        completedPoll_.completedAtMs = millis();
        pollCompletionPending_ = true;
    }

    void advancePollChannel() {
        if (channels_.empty()) {
            pollChannelIndex_ = 0;
            return;
        }
        pollChannelIndex_ = (pollChannelIndex_ + 1U) % channels_.size();
    }

    static bool parsePositiveNumber(const String& value, uint16_t& number) {
        if (value.isEmpty()) return false;
        uint32_t parsed = 0;
        for (size_t i = 0; i < value.length(); ++i) {
            const char c = value.charAt(i);
            if (c < '0' || c > '9') return false;
            parsed = parsed * 10U + static_cast<uint32_t>(c - '0');
            if (parsed > 65535U) return false;
        }
        if (parsed == 0) return false;
        number = static_cast<uint16_t>(parsed);
        return true;
    }

    static bool parsePointId(const String& pointId, uint8_t& slot, uint8_t& registerIndex) {
        static const char* prefix = "compat/";
        if (!pointId.startsWith(prefix)) return false;

        const String suffix = pointId.substring(strlen(prefix));
        const int slash = suffix.indexOf('/');
        const String channelPart = slash < 0 ? suffix : suffix.substring(0, slash);

        uint16_t channelNumber = 0;
        if (!parsePositiveNumber(channelPart, channelNumber) ||
            channelNumber > modbus::kCompatibilitySlotCount) {
            return false;
        }

        registerIndex = 0;
        if (slash >= 0) {
            if (suffix.indexOf('/', slash + 1) >= 0) return false;
            uint16_t registerNumber = 0;
            if (!parsePositiveNumber(suffix.substring(slash + 1), registerNumber) || registerNumber > 2) {
                return false;
            }
            registerIndex = static_cast<uint8_t>(registerNumber - 1U);
        }

        slot = static_cast<uint8_t>(channelNumber - 1U);
        return true;
    }

    ModbusMode mode_ = ModbusMode::Disabled;
    bool active_ = false;
    modbus::Rs485SerialSettings rs485Settings_;
    modbus::ModbusMasterSettings masterSettings_;
    modbus::RtuSerialConfig serialConfig_;
    modbus::ModbusRtuMaster rtuMaster_;
    std::vector<modbus::ChannelConfig> channels_;
    PollCache cache_[modbus::kCompatibilitySlotCount];
    PollCompletion completedPoll_;
    bool pollCompletionPending_ = false;
    size_t pollChannelIndex_ = 0;
    uint8_t pollRetryCount_ = 0;
    uint32_t nextPollAtMs_ = 0;
};

class GnssComponent final : public Component, public DataSource {
public:
    void setMode(GnssMode mode) { mode_ = mode; active_ = false; }
    const char* name() const override { return "gnss"; }
    const char* sourceId() const override { return "gnss:primary"; }

    bool begin(CapabilityRegistry& capabilities) override {
        if (mode_ != GnssMode::Enabled) return true;
        capabilities.add("source.gnss");
        capabilities.add("gnss.position");
        capabilities.add("gnss.time");
        capabilities.add("gnss.motion");
        active_ = true;
        return true;
    }

    void loop() override {}
    bool active() const { return active_; }
    bool online() const override { return false; }
    size_t pointCount() const override { return 0; }
    bool describePoint(size_t, DataPointDescriptor&) const override { return false; }
    bool readPoint(const String&, DataValue&) override { return false; }
    bool writePoint(const String&, const DataValue&) override { return false; }

private:
    GnssMode mode_ = GnssMode::Disabled;
    bool active_ = false;
};

} // namespace multibus
