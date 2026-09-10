#pragma once

#include <Arduino.h>
#include "AppConfig.h"
#include "CapabilityRegistry.h"
#include "components/ComponentStubs.h"

namespace multibus {

class Application {
public:
    explicit Application(const AppConfig& config)
        : config_(config),
          victron_(config.victron),
          lora_(config.lora),
          modbus_(config.modbus),
          gnss_(config.gnss) {}

    bool begin() {
        const auto validation = validateConfig(config_);
        if (validation != ConfigValidationResult::Ok) {
            Serial.printf("Configuration rejected: %s\n", toString(validation));
            return false;
        }

        capabilities_.clear();
        capabilities_.add("core.config");
        capabilities_.add("core.capabilities");
        capabilities_.add("core.rules");
        capabilities_.add("core.history");
        capabilities_.add("core.webui");

        if (!beginComponent(victron_)) return false;
        if (!beginComponent(lora_)) return false;
        if (!beginComponent(modbus_)) return false;
        if (!beginComponent(gnss_)) return false;

        printStatus();
        return true;
    }

    void loop() {
        victron_.loop();
        lora_.loop();
        modbus_.loop();
        gnss_.loop();
    }

    const AppConfig& config() const { return config_; }
    const CapabilityRegistry& capabilities() const { return capabilities_; }

private:
    bool beginComponent(Component& component) {
        if (!component.begin(capabilities_)) {
            Serial.printf("Component failed to start: %s\n", component.name());
            return false;
        }
        return true;
    }

    void printStatus() const {
        Serial.println("MultiBus component configuration:");
        Serial.printf("  V: %s\n", toString(config_.victron));
        Serial.printf("  L: %s\n", toString(config_.lora));
        Serial.printf("  M: %s\n", toString(config_.modbus));
        Serial.printf("  G: %s\n", toString(config_.gnss));
        Serial.println("Capabilities:");
        for (const auto& capability : capabilities_.all()) {
            Serial.printf("  - %s\n", capability.c_str());
        }
    }

    AppConfig config_;
    CapabilityRegistry capabilities_;
    VictronComponent victron_;
    LoRaComponent lora_;
    ModbusComponent modbus_;
    GnssComponent gnss_;
};

} // namespace multibus
