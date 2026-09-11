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
    void setMode(ModbusMode mode) {
        mode_ = mode;
        active_ = false;
    }

    void setRtuSerialConfig(const modbus::RtuSerialConfig& config) {
        serialConfig_ = config;
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
                capabilities.add("modbus.compatibility-channels");
                if (!rtuMaster_.begin(serialConfig_)) return false;
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

    void loop() override {}
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

        modbus::DecodedScalar values[2];
        uint8_t valueCount = 0;
        if (!rtuMaster_.read(*channel, values, valueCount) || registerIndex >= valueCount) return false;

        value.valid = true;
        const auto& scalar = values[registerIndex];
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

    const modbus::ModbusRtuMaster& rtuMaster() const { return rtuMaster_; }

private:
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
    modbus::RtuSerialConfig serialConfig_;
    modbus::ModbusRtuMaster rtuMaster_;
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
