#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "board/BoardPins.h"
#include "core/ConfigStore.h"
#include "core/SecurityStore.h"

namespace multibus {

class BoardService {
public:
    bool begin(ConfigStore& configStore, SecurityStore& securityStore) {
        configStore_ = &configStore;
        securityStore_ = &securityStore;

        pinMode(board::USER_BUTTON, INPUT_PULLUP);
        pinMode(board::STATUS_LED, OUTPUT);
        digitalWrite(board::STATUS_LED, LOW);

        // Factory reset is only armed after the button has been observed released
        // once after boot, preventing a held boot/program button from erasing the
        // device immediately during startup.
        resetArmed_ = digitalRead(board::USER_BUTTON) == HIGH;
        return true;
    }

    void loop() {
        const bool pressed = digitalRead(board::USER_BUTTON) == LOW;

        if (!resetArmed_) {
            if (!pressed) resetArmed_ = true;
            return;
        }

        if (!pressed) {
            pressStartedAt_ = 0;
            resetWarning_ = false;
            digitalWrite(board::STATUS_LED, LOW);
            return;
        }

        if (pressStartedAt_ == 0) {
            pressStartedAt_ = millis();
        }

        const uint32_t held = millis() - pressStartedAt_;
        resetWarning_ = held >= kWarningAfterMs;

        if (resetWarning_) {
            // Visible fallback until OLED UI is implemented.
            digitalWrite(board::STATUS_LED, ((held / 200) % 2) == 0 ? HIGH : LOW);
        }

        if (held >= kFactoryResetAfterMs) {
            performFactoryReset();
        }
    }

    bool factoryResetWarningActive() const { return resetWarning_; }

    uint32_t factoryResetHoldMs() const {
        if (pressStartedAt_ == 0) return 0;
        return millis() - pressStartedAt_;
    }

private:
    void performFactoryReset() {
        digitalWrite(board::STATUS_LED, HIGH);

        if (configStore_ != nullptr) configStore_->clear();
        if (securityStore_ != nullptr) securityStore_->clear();

        // Clear any Wi-Fi credentials cached by the ESP32 Wi-Fi stack as well.
        WiFi.disconnect(true, true);
        delay(250);
        ESP.restart();
    }

    static constexpr uint32_t kWarningAfterMs = 3000;
    static constexpr uint32_t kFactoryResetAfterMs = 10000;

    ConfigStore* configStore_ = nullptr;
    SecurityStore* securityStore_ = nullptr;
    bool resetArmed_ = false;
    bool resetWarning_ = false;
    uint32_t pressStartedAt_ = 0;
};

} // namespace multibus
