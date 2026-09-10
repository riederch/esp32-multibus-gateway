# ESP32 MultiBus Gateway

MultiBus Gateway is a modular ESP32-S3 field gateway for LoRaWAN, RS485/Modbus, Victron VE.Bus, GNSS and local automation.

## Reference hardware

- Heltec HTIT-WB32LAF V4.2 / WiFi LoRa 32 V4.2
- ESP32-S3
- SX1262, EU868
- OLED display
- Wi-Fi and Bluetooth LE
- native USB-C
- BAT and SOL connections
- external GNSS interface
- isolated RS485 interface for Modbus
- isolated RS485 interface for VE.Bus

## Component modes

```text
V = [1,0]       Victron enabled / disabled
L = [W,M,0]     LoRaWAN / Meshtastic / disabled
M = [M,S,0]     Modbus RTU master / slave / disabled
G = [1,0]       GNSS enabled / disabled
```

LoRaWAN and Meshtastic are mutually exclusive because they use the same SX1262 radio.

## Core model

The firmware is built around a shared Core and independent components:

```text
                    +-----------------------+
                    |         Core          |
                    | config / channels     |
                    | rules / history       |
                    | security / Web UI     |
                    +-----------+-----------+
                                |
             +------------------+------------------+
             |                  |                  |
         DataSource         DataSource         DataSource
          Modbus             Victron             GNSS
             |                  |                  |
           RS485              VE.Bus              UART
                                |
                         +------+------+
                         | LoRa transport |
                         | LoRaWAN / Mesh |
                         +---------------+
```

Modbus, Victron, GNSS and platform I/O expose values through a common data-source/channel abstraction. LoRaWAN, history, alarms, rules and the Web UI consume channels rather than bus-specific internals.

## LoRaWAN protocol

The LoRaWAN interface provides a compatibility profile on **FPort 85** that follows the Milesight UC100 V2 wire protocol. Existing compatible payload decoders and downlink generators can therefore be reused for the standard command set.

MultiBus-specific functions use a separate configurable extension FPort and never redefine FPort-85 commands. A Victron value can be bound to a normal compatibility channel and is then reported through the same channel/alarm/history framing as a Modbus-derived value.

Milesight D2D radio operation is not part of the platform. D2D-related protocol identifiers remain reserved so they cannot collide with MultiBus extensions. Meshtastic is the optional mesh backend under `L=M`.

See `docs/lorawan-protocol.md`.

## Field interfaces

### 4-pole terminal

```text
V+   5-30 V DC supply
V-   supply return
A    Modbus RS485 A
B    Modbus RS485 B
```

### Victron RJ45

The optional RJ45 carries VE.Bus communication and may also provide a power source through VE.Bus V+/GND. VE.Bus and Modbus use independent isolated transceiver paths.

External power and VE.Bus-derived power must be protected against back-feed.

## Administration

Wi-Fi and the local Web UI are the primary administration interface.

- WLAN client mode exposes the Web UI through the assigned IP address and mDNS hostname.
- AP commissioning mode displays SSID, AP password and configuration address on the OLED.
- A one-time administrator password is displayed during initial provisioning.
- The first administrator login requires a password change.
- A deliberate long button press performs a complete factory reset.
- Configuration backup and restore are available through the Web UI/backend.

USB-C remains available for flashing, service and recovery. When Victron is enabled, USB and BLE may additionally expose Victron compatibility functions.

## Board services

The Core exposes board resources independently of protocol components:

- OLED
- user button and LED
- battery voltage measurement
- BAT / SOL power facilities
- switchable Vext
- GPIO / ADC / PWM / touch
- I2C / SPI / available UART resources
- Wi-Fi / BLE / USB
- persistent storage
- watchdog
- OTA/update services

## Documentation

- `docs/specification.md` - product requirements
- `docs/architecture.md` - component, channel and data-source architecture
- `docs/lorawan-protocol.md` - LoRaWAN compatibility profile and extensions
- `docs/hardware.md` - hardware and electrical interfaces
- `docs/rule-engine.md` - automation model
- `docs/web-ui.md` - Wi-Fi, Web UI and OLED interaction
- `docs/security-and-provisioning.md` - authentication, reset and backup/restore
- `docs/validation.md` - unresolved technical validation items
- `docs/backlog.md` - deferred optional features
