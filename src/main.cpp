#include <Arduino.h>
#include "config.h"

HardwareSerial VeBusSerial(cfg::VEBUS_UART_NUM);
HardwareSerial ModbusSerial(cfg::MODBUS_UART_NUM);

static void initBusPins() {
    pinMode(cfg::VEBUS_DE_PIN, OUTPUT);
    digitalWrite(cfg::VEBUS_DE_PIN, LOW);

    pinMode(cfg::MODBUS_DE_PIN, OUTPUT);
    digitalWrite(cfg::MODBUS_DE_PIN, LOW);
}

static void initSerialBuses() {
    VeBusSerial.begin(
        cfg::VEBUS_BAUD,
        SERIAL_8N1,
        cfg::VEBUS_RX_PIN,
        cfg::VEBUS_TX_PIN
    );

    ModbusSerial.begin(
        cfg::MODBUS_BAUD,
        SERIAL_8E1,
        cfg::MODBUS_RX_PIN,
        cfg::MODBUS_TX_PIN
    );
}

void setup() {
    Serial.begin(115200);
    delay(500);

    initBusPins();
    initSerialBuses();

    Serial.println();
    Serial.println("ESP32 MultiBus Gateway");
    Serial.println("VE.Bus and Modbus UARTs initialized.");
    Serial.println("No protocol traffic is transmitted yet.");
}

void loop() {
    // Intentionally passive for the initial scaffold.
    // VE.Bus and Modbus protocol implementations will be added separately.
    delay(1000);
}
