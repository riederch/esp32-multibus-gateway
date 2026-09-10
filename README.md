# ESP32 MultiBus Gateway

ESP32-S3 based gateway and development platform for two independent galvanically isolated serial field buses:

- **VE.Bus** for Victron MultiPlus devices
- **RS485 / Modbus RTU** for generic field devices

Planned higher-level interfaces:

- USB-C diagnostics and, later, experimental MK2/MK3-compatible transport
- Wi-Fi Web UI
- MQTT
- Bluetooth LE experiments for VictronConnect-compatible functionality

## Project goals

1. Establish reliable direct VE.Bus communication with a Victron MultiPlus.
2. Read VE.Bus measurements and settings.
3. Write selected VE.Bus settings, initially including charger current.
4. Provide an independent Modbus RTU master/slave capable RS485 port.
5. Expose data and configuration via Web UI and MQTT.
6. Investigate USB MK2/MK3 protocol emulation.
7. Investigate VE.Bus Smart Dongle BLE emulation as an optional later feature.

## Hardware concept

```text
                       ESP32-S3
                          |
          +---------------+---------------+
          |                               |
        UART1                           UART2
          |                               |
   isolated RS485                 isolated RS485
          |                               |
        VE.Bus                         Modbus RTU
         RJ45                          A / B / GND
```

The two bus interfaces are intentionally independent. VE.Bus and Modbus must **not** share one switched RS485 transceiver.

For the first prototype, two ready-made isolated TTL-to-RS485 modules can be used. A later PCB should use two separately isolated RS485 channels with explicit driver-enable control, especially on VE.Bus.

## Current target device

Initial VE.Bus development target:

- Victron MultiPlus 12/500/20-16

Initial test objective:

- read charger configuration
- safely reduce charger current for testing with a small 12 V lead-acid battery

## Status

Early project scaffold. No VE.Bus device should be connected until the electrical interface, RJ45 pinout, termination/biasing and isolation have been verified.

## Repository layout

```text
include/                 shared configuration and interfaces
src/                     firmware
  vebus/                 VE.Bus transport/protocol
  modbus/                Modbus RTU transport
  services/              Web/MQTT/BLE/USB services
  main.cpp                application entry point
docs/                    design and research notes
hardware/                hardware notes, schematics and PCB files later
```

## Safety

VE.Bus is a proprietary Victron bus associated with inverter/charger equipment connected to battery and mains systems. Direct communication is experimental and is not an officially supported substitute for Victron MK2/MK3 accessories. Maintain galvanic isolation and do not connect experimental hardware to live mains-powered equipment until the interface has been validated.
