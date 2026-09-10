#pragma once

#include <Arduino.h>
#include "AppConfig.h"
#include "CapabilityRegistry.h"
#include "ConfigStore.h"
#include "DeviceConfig.h"
#include "DeviceIdentity.h"
#include "SecurityStore.h"
#include "components/Components.h"
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

        if (!configStore_.load(config_)) {
            config_ = DeviceConfig{};
            config_.network.hostname = defaultHostname();
            config_.network.friendlyName = "MultiBus Gateway";
            if (!configStore_.save(config_)) {
                Serial.println("Failed to initialize default configuration.");
                return false;
            }
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

        configureComponents();
        registerCoreCapabilities();

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
    const SecurityStore& security() const { return security_; }
    const NetworkService& network() const { return network_; }
    const BoardService& board() const { return board_; }

private:
    void configureComponents() {
        victron_.setMode(config_.components.victron);
        lora_.setMode(config_.components.lora);
        modbus_.setMode(config_.components.modbus);
        gnss_.setMode(config_.components.gnss);
    }

    void registerCoreCapabilities() {
        capabilities_.clear();
        capabilities_.add("core.config");
        capabilities_.add("core.capabilities");
        capabilities_.add("core.data-sources");
        capabilities_.add("core.channels");
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
    ConfigStore configStore_;
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
