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
#include "modbus/ModbusChannel.h"

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
    using DownlinkHandler = bool (*)(void* context, uint8_t fport, const uint8_t* payload, size_t length);

    void setMode(LoRaMode mode) { mode_ = mode; active_ = false; }

    void setProvisioning(const LoRaWanConfig& config, const String& devEuiValue) {
        lorawan_ = config;
        devEui_ = devEuiValue;
    }

    void setDownlinkHandler(DownlinkHandler handler, void* context) {
        downlinkHandler_ = handler;
        downlinkContext_ = context;
    }

    bool dispatchDownlink(uint8_t fport, const uint8_t* payload, size_t length) {
        if (!active_ || downlinkHandler_ == nullptr || payload == nullptr || length == 0) return false;
        return downlinkHandler_(downlinkContext_, fport, payload, length);
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

        active_ = true;
        provisioned_ = isHexString(devEui_, 16) && validateLoRaWanConfig(lorawan_);
        return true;
    }

    void loop() override {}
    bool active() const { return active_; }
    bool provisioned() const { return provisioned_; }
    bool connected() const override { return false; }
    bool send(const TransportEnvelope&) override { return false; }

    const String& devEuiValue() const { return devEui_; }
    const LoRaWanConfig& provisioning() const { return lorawan_; }

private:
    LoRaMode mode_ = LoRaMode::Disabled;
    LoRaWanConfig lorawan_;
    String devEui_;
    DownlinkHandler downlinkHandler_ = nullptr;
    void* downlinkContext_ = nullptr;
    bool active_ = false;
    bool provisioned_ = false;
};

class ModbusComponent final : public Component, public DataSource {
public:
    void setMode(ModbusMode mode) { mode_ = mode; active_ = false; }
    const char* name() const override { return "modbus"; }
    const char* sourceId() const override { return "modbus"; }

    bool begin(CapabilityRegistry& capabilities) override {
        switch (mode_) {
            case ModbusMode::Master:
                capabilities.add("source.modbus");
                capabilities.add("modbus.master");
                capabilities.add("modbus.read");
                capabilities.add("modbus.write");
                capabilities.add("modbus.raw");
                capabilities.add("modbus.compatibility-channels");
                active_ = true;
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

    static String pointIdForSlot(uint8_t slot) {
        char value[16];
        snprintf(value, sizeof(value), "compat/%u", static_cast<unsigned>(slot + 1));
        return String(value);
    }

    void loop() override {}
    bool active() const { return active_; }
    bool online() const override { return false; }
    size_t pointCount() const override { return channels_.size(); }

    bool describePoint(size_t index, DataPointDescriptor& descriptor) const override {
        if (index >= channels_.size()) return false;
        const auto& channel = channels_[index];
        descriptor.id = pointIdForSlot(channel.slot);
        descriptor.unit = "";
        descriptor.readable = true;
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

    bool readPoint(const String&, DataValue&) override { return false; }
    bool writePoint(const String&, const DataValue&) override { return false; }

private:
    ModbusMode mode_ = ModbusMode::Disabled;
    bool active_ = false;
    std::vector<modbus::ChannelConfig> channels_;
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
