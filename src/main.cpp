#include <Arduino.h>
#include "config.h"
#include "core/Application.h"

using namespace multibus;

namespace {

Application app;
bool appReady = false;

} // namespace

void setup() {
    Serial.begin(cfg::SERIAL_MONITOR_BAUD);
    delay(300);

    Serial.println();
    Serial.println("ESP32 MultiBus Gateway");
    Serial.println("Starting platform services...");

    appReady = app.begin();
    if (!appReady) {
        Serial.println("Application startup failed; protocol hardware remains inactive.");
    }
}

void loop() {
    if (appReady) {
        app.loop();
    }

    delay(2);
}
