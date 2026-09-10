#include <Arduino.h>
#include "core/AppConfig.h"
#include "core/Application.h"

using namespace multibus;

namespace {

// Safe development default: all optional protocol components are disabled.
// Runtime persistence / Web UI configuration will replace this static bootstrap
// configuration in the next implementation step.
AppConfig appConfig{
    VictronMode::Disabled,
    LoRaMode::Disabled,
    ModbusMode::Disabled,
    GnssMode::Disabled,
};

Application app(appConfig);
bool appReady = false;

} // namespace

void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("ESP32 MultiBus Gateway");
    Serial.println("Starting modular application core...");

    appReady = app.begin();
    if (!appReady) {
        Serial.println("Application startup failed; protocol hardware remains inactive.");
    }
}

void loop() {
    if (appReady) {
        app.loop();
    }

    delay(10);
}
