#pragma once

#include <Arduino.h>
#include <vector>
#include "AppConfig.h"
#include "CapabilityRegistry.h"
#include "ChannelRegistry.h"
#include "ConfigStore.h"
#include "DeviceConfig.h"
#include "DeviceIdentity.h"
#include "LoRaWanIdentity.h"
#include "SecurityStore.h"
#include "components/Components.h"
#include "lorawan/FPort85Codec.h"
#include "modbus/ModbusChannelStore.h"
#include "services/BoardService.h"
#include "services/NetworkService.h"
#include "services/WebService.h"

namespace multibus {

class Application {
public:
    bool begin() {
        if (!configStore_.begin()) {
            Serial.println("Failed to open configuration store.");
            return false;
        }
        if (!modbusChannelStore_.begin()) {
            Serial.println("Failed to open Modbus channel store.");
            return false;
        }

        bool configChanged = false;
        if (!configStore_.load(config_)) {
            config_ = DeviceConfig{};
            config_.network.hostname = defaultHostname();
            config_.network.friendlyName = "MultiBus Gateway";
            configChanged = true;
        }

        if (ensureLoRaWanConfig(config_.lorawan)) {
            configChanged = true;
        }
        if (configStore_.requiresSave()) {
            configChanged = true;
        }
        if (configChanged && !configStore_.save(config_)) {
            Serial.println("Failed to initialize or migrate configuration.");
            return false;
        }

        if (!security_.begin()) {
            Serial.println("Failed to initialize security store.");
            return false;
        }

        if (!board_.begin(configStore_, security_)) {
            Serial.println("Failed to initialize board service.");
            return false;
        }

        const auto validation = validateConfig(config_.components);
        if (validation != ConfigValidationResult::Ok) {
            Serial.printf("Configuration rejected: %s\n", toString(validation));
            return false;
        }
        if (!validateLoRaWanConfig(config_.lorawan)) {
            Serial.println("LoRaWAN configuration rejected.");
            return false;
        }

        configureComponents();
        lora_.setDownlinkHandler(&Application::downlinkThunk, this);
        registerCoreCapabilities();

        if (!loadModbusChannels()) {
            Serial.println("Failed to load persisted Modbus channels.");
            return false;
        }

        if (!beginComponent(victron_)) return false;
        if (!beginComponent(lora_)) return false;
        if (!beginComponent(modbus_)) return false;
        if (!beginComponent(gnss_)) return false;

        if (!network_.begin(config_, security_)) {
            Serial.println("Failed to initialize Wi-Fi networking.");
            return false;
        }

        capabilities_.add("board.user-button");
        capabilities_.add("board.status-led");
        capabilities_.add("board.factory-reset");
        capabilities_.add("network.wifi");
        capabilities_.add("network.webui");
        if (network_.apActive()) capabilities_.add("network.ap");
        if (network_.clientConnected()) capabilities_.add("network.client");

        if (!web_.begin(config_, configStore_, security_, network_)) {
            Serial.println("Failed to initialize Web UI.");
            return false;
        }

        printStatus();
        return true;
    }

    void loop() {
        board_.loop();
        network_.loop();
        web_.loop();
        victron_.loop();
        lora_.loop();
        modbus_.loop();
        gnss_.loop();
    }

    const DeviceConfig& config() const { return config_; }
    const CapabilityRegistry& capabilities() const { return capabilities_; }
    const ChannelRegistry& channels() const { return channels_; }
    const SecurityStore& security() const { return security_; }
    const NetworkService& network() const { return network_; }
    const BoardService& board() const { return board_; }

private:
    static bool downlinkThunk(void* context, uint8_t fport, const uint8_t* payload, size_t length) {
        if (context == nullptr) return false;
        return static_cast<Application*>(context)->handleLoRaDownlink(fport, payload, length);
    }

    bool handleLoRaDownlink(uint8_t fport, const uint8_t* payload, size_t length) {
        if (fport != lorawan::kCompatibilityFPort || payload == nullptr || length == 0) return false;

        std::vector<lorawan::ModbusChannelCommand> commands;
        size_t offset = 0;
        while (offset < length) {
            lorawan::CommandHeader header;
            if (!lorawan::FPort85Codec::readHeader(payload + offset, length - offset, header)) return false;
            if (header.channelId != lorawan::FPort85Codec::kSystemChannel ||
                header.type != lorawan::FPort85Codec::kModbusChannelConfigType) {
                return false;
            }

            lorawan::ModbusChannelCommand command;
            size_t consumed = 0;
            if (lorawan::FPort85Codec::decodeModbusChannelCommand(
                    payload + offset, length - offset, command, consumed) != lorawan::DecodeStatus::Ok ||
                consumed == 0) {
                return false;
            }
            commands.push_back(command);
            offset += consumed;
        }

        for (const auto& command : commands) {
            if (!applyModbusChannelCommand(command)) return false;
        }
        return true;
    }

    bool applyModbusChannelCommand(const lorawan::ModbusChannelCommand& command) {
        switch (command.operation) {
            case lorawan::ModbusChannelOperation::Upsert:
                if (!modbusChannelStore_.save(command.channel)) return false;
                if (!modbus_.upsertChannel(command.channel)) return false;
                return bindCompatibilityChannel(command.channel);

            case lorawan::ModbusChannelOperation::Remove:
                if (!modbusChannelStore_.remove(command.channel.slot)) return false;
                modbus_.removeChannel(command.channel.slot);
                channels_.removeCompatibilitySlot(command.channel.slot);
                return true;

            case lorawan::ModbusChannelOperation::SetName: {
                const modbus::ChannelConfig* current = modbus_.channelForSlot(command.channel.slot);
                if (current == nullptr) return false;
                modbus::ChannelConfig updated = *current;
                const size_t nameLength = strnlen(command.channel.name, modbus::kMaxChannelNameLength);
                if (!modbus::setChannelName(updated, command.channel.name, nameLength)) return false;
                if (!modbusChannelStore_.save(updated)) return false;
                return modbus_.upsertChannel(updated);
            }
        }
        return false;
    }

    bool loadModbusChannels() {
        std::vector<modbus::ChannelConfig> stored;
        if (!modbusChannelStore_.load(stored)) return false;
        for (const auto& channel : stored) {
            if (!modbus_.upsertChannel(channel) || !bindCompatibilityChannel(channel)) return false;
        }
        return true;
    }

    bool bindCompatibilityChannel(const modbus::ChannelConfig& channel) {
        ChannelBinding binding;
        binding.channelId = static_cast<uint16_t>(channel.slot) + 1U;
        binding.sourceId = modbus_.sourceId();
        binding.pointId = ModbusComponent::pointIdForSlot(channel.slot);
        binding.enabled = true;
        binding.writable = false;
        binding.compatibilityMapped = true;
        binding.compatibilitySlot = channel.slot;
        return channels_.upsert(binding);
    }

    void configureComponents() {
        victron_.setMode(config_.components.victron);
        lora_.setMode(config_.components.lora);
        lora_.setProvisioning(config_.lorawan, devEui());
        modbus_.setMode(config_.components.modbus);
        gnss_.setMode(config_.components.gnss);
    }

    void registerCoreCapabilities() {
        capabilities_.clear();
        capabilities_.add("core.config");
        capabilities_.add("core.capabilities");
        capabilities_.add("core.data-sources");
        capabilities_.add("core.transports");
        capabilities_.add("core.channels");
        capabilities_.add("core.events");
        capabilities_.add("core.commands");
        capabilities_.add("core.rules");
        capabilities_.add("core.history");
        capabilities_.add("core.security");
        capabilities_.add("core.backup-restore");
    }

    bool beginComponent(Component& component) {
        if (!component.begin(capabilities_)) {
            Serial.printf("Component failed to start: %s\n", component.name());
            return false;
        }
        return true;
    }

    void printStatus() const {
        Serial.println("MultiBus component configuration:");
        Serial.printf("  V: %s\n", toString(config_.components.victron));
        Serial.printf("  L: %s\n", toString(config_.components.lora));
        Serial.printf("  M: %s\n", toString(config_.components.modbus));
        Serial.printf("  G: %s\n", toString(config_.components.gnss));
        Serial.printf("  DevEUI: %s\n", devEui().c_str());
        Serial.printf("  JoinEUI: %s\n", config_.lorawan.joinEui.c_str());
        Serial.printf("  LoRaWAN provisioned: %s\n", lora_.provisioned() ? "yes" : "no");
        Serial.printf("  Compatibility channels: %u\n", static_cast<unsigned>(channels_.size()));
        Serial.printf("  Wi-Fi: %s\n", network_.apActive() ? "commissioning AP" : "client");
        Serial.printf("  Address: %s\n", network_.address().toString().c_str());
        Serial.println("Capabilities:");
        for (const auto& capability : capabilities_.all()) {
            Serial.printf("  - %s\n", capability.c_str());
        }

        if (security_.hasInitialAdminPasswordForDisplay()) {
            Serial.println("Initial administrator password pending display output.");
        }
    }

    DeviceConfig config_;
    CapabilityRegistry capabilities_;
    ChannelRegistry channels_;
    ConfigStore configStore_;
    modbus::ModbusChannelStore modbusChannelStore_;
    SecurityStore security_;
    BoardService board_;
    NetworkService network_;
    WebService web_;
    VictronComponent victron_;
    LoRaComponent lora_;
    ModbusComponent modbus_;
    GnssComponent gnss_;
};

} // namespace multibus
