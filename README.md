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

Protocol adapters connect to the Core in distinct roles. Modbus, Victron, GNSS and platform I/O provide normalized data points. LoRa provides transport for telemetry, events, commands and remote configuration.

```text
                         +---------------------------+
                         |           CORE            |
                         | sources / channels        |
                         | events / commands         |
                         | rules / history           |
                         | config / security / UI    |
                         +-------------+-------------+
                                       |
        +------------------------------+------------------------------+
        |                              |                              |
        v                              v                              v
 +--------------+              +--------------+               +--------------+
 | Data sources |              |  Consumers   |               |  Transports  |
 +------+-------+              +------+-------+               +------+-------+
        |                             |                              |
   +----+----+----+              +----+----+                   +-----+-----+
   |         |    |              |         |                   |           |
 Modbus   Victron GNSS          Rules    History               LoRa      future
  RS485    VE.Bus UART/PPS       Web UI                         |
                                                             +---+---+
                                                             |       |
                                                          LoRaWAN  Mesh
```

A component can expose one or more roles, but LoRa is not a child of Victron or VE.Bus. All cross-component communication goes through Core abstractions.

## Channel model

A channel binds a normalized source point to reporting, alarm, history and remote-control behaviour.

Examples:

```text
modbus:12/pressure.bar          -> channel 1
victron:vebus/battery.voltage  -> channel 20
gnss:primary/speed             -> channel 30
```

LoRaWAN, rules, history and the Web UI consume channels rather than bus-specific frame formats.

## LoRaWAN protocol

LoRaWAN uses OTAA as the primary activation method. Device identity consists of a stable DevEUI, a configured JoinEUI and an individual 128-bit AppKey.

The compatibility profile on **FPort 85** follows the Milesight UC100 V2 wire protocol. Existing compatible payload decoders and downlink generators can therefore be reused for its standard command set. MultiBus-specific functions use a separate configurable extension FPort and never redefine FPort-85 commands.

A Victron, GNSS or platform value can be bound to a logical compatibility channel and is then reported through the same channel/alarm/history framing as a Modbus-derived value.

Milesight D2D radio operation is not implemented. D2D-related protocol identifiers remain reserved. Meshtastic is the optional mesh backend under `L=M`.

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

## Firmware distribution

Every successful `main` build publishes `firmware.bin` and its SHA-256 digest as a GitHub Actions artifact. Version tags publish a GitHub Release containing a versioned firmware binary and digest.

The Web UI firmware updater will accept the application `.bin` for OTA installation. Signed firmware/rollback policy remains a validation and hardening item.

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
- `docs/architecture.md` - source, channel, transport and component architecture
- `docs/lorawan-protocol.md` - LoRaWAN compatibility, identity and extensions
- `docs/hardware.md` - hardware and electrical interfaces
- `docs/rule-engine.md` - automation model
- `docs/web-ui.md` - Wi-Fi, Web UI and OLED interaction
- `docs/security-and-provisioning.md` - authentication, reset and backup/restore
- `docs/validation.md` - unresolved technical validation items
- `docs/backlog.md` - deferred optional features
