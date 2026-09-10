#pragma once

#include <Arduino.h>
#include "core/AppConfig.h"
#include "core/CapabilityRegistry.h"
#include "core/Component.h"

namespace multibus {

class VictronComponent final : public Component {
public:
    void setMode(VictronMode mode) { mode_ = mode; active_ = false; }
    const char* name() const override { return "victron"; }
    bool begin(CapabilityRegistry& capabilities) override {
        if (mode_ != VictronMode::Enabled) return true;
        capabilities.add("victron.telemetry");
        capabilities.add("victron.settings");
        capabilities.add("victron.usb.mk3");
        capabilities.add("victron.ble.victronconnect");
        active_ = true;
        return true;
    }
    void loop() override {}
    bool active() const { return active_; }
private:
    VictronMode mode_ = VictronMode::Disabled;
    bool active_ = false;
};

class LoRaComponent final : public Component {
public:
    void setMode(LoRaMode mode) { mode_ = mode; active_ = false; }
    const char* name() const override { return "lora"; }
    bool begin(CapabilityRegistry& capabilities) override {
        if (mode_ == LoRaMode::Disabled) return true;
        if (mode_ == LoRaMode::Meshtastic) return false;
        capabilities.add("lora.lorawan");
        capabilities.add("lora.uplink");
        capabilities.add("lora.downlink");
        active_ = true;
        return true;
    }
    void loop() override {}
    bool active() const { return active_; }
private:
    LoRaMode mode_ = LoRaMode::Disabled;
    bool active_ = false;
};

class ModbusComponent final : public Component {
public:
    void setMode(ModbusMode mode) { mode_ = mode; active_ = false; }
    const char* name() const override { return "modbus"; }
    bool begin(CapabilityRegistry& capabilities) override {
        switch (mode_) {
            case ModbusMode::Master:
                capabilities.add("modbus.master");
                capabilities.add("modbus.read");
                capabilities.add("modbus.write");
                capabilities.add("modbus.raw");
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
    void loop() override {}
    bool active() const { return active_; }
private:
    ModbusMode mode_ = ModbusMode::Disabled;
    bool active_ = false;
};

class GnssComponent final : public Component {
public:
    void setMode(GnssMode mode) { mode_ = mode; active_ = false; }
    const char* name() const override { return "gnss"; }
    bool begin(CapabilityRegistry& capabilities) override {
        if (mode_ != GnssMode::Enabled) return true;
        capabilities.add("gnss.position");
        capabilities.add("gnss.time");
        capabilities.add("gnss.motion");
        active_ = true;
        return true;
    }
    void loop() override {}
    bool active() const { return active_; }
private:
    GnssMode mode_ = GnssMode::Disabled;
    bool active_ = false;
};

} // namespace multibus
