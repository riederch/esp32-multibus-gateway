# ESP32 MultiBus Gateway

A modular ESP32-S3 based multi-purpose fieldbus and LoRa platform.

The project is no longer defined around Victron as a mandatory interface. Instead, it consists of independently configurable components with a shared core for configuration, automation, history, UI and maintenance.

Reference prototype hardware:

- **Heltec HTIT-WB32LAF V4.2** / WiFi LoRa 32 V4.2
- ESP32-S3
- SX1262 for EU868 LoRa
- optional external GNSS module
- OLED display
- native USB-C
- Wi-Fi and Bluetooth LE
- external isolated RS485 interfaces for Modbus and optional Victron VE.Bus

## Component modes

```text
V = [1,0]
    1 = complete Victron component enabled
    0 = Victron disabled

L = [W,M,0]
    W = LoRaWAN
    M = Meshtastic [backlog / not implemented]
    0 = LoRa radio disabled

M = [M,S,0]
    M = Modbus RTU master
    S = Modbus RTU slave
    0 = Modbus disabled

G = [1,0]
    1 = GNSS enabled
    0 = GNSS disabled
```

The modes are independent except where a physical hardware resource is shared. LoRaWAN and Meshtastic are mutually exclusive because they use the same SX1262 radio.

## Primary use cases

- UC100-like Modbus RTU master -> LoRaWAN gateway
- LoRaWAN -> Modbus RTU slave bridge
- Victron + Modbus telemetry -> LoRaWAN
- Victron -> LoRaWAN
- standalone Victron compatibility adapter
- local Modbus gateway/logger
- GNSS-enabled mobile or stationary telemetry node
- later: Meshtastic-backed variants of the same application model

## Victron component

Victron is implemented as a closed compatibility block. When `V=1`, the component owns the complete Victron-specific functionality:

- direct VE.Bus communication
- settings and telemetry
- optional Standby / Panel Detect support
- VictronConnect-compatible BLE experiments
- MK2/MK3 protocol engine
- native USB MK3-USB compatibility experiments

Other components never access VE.Bus internals directly. They interact only through the Victron component's defined properties, events and commands.

## UC100 replacement target

The platform should reproduce the useful UC100 feature set without Milesight D2D:

- configurable Modbus channels
- polling and register conversion
- data types, byte/word order, scale and offset
- transparent/raw RS485 access
- threshold and change alarms
- generic IF/THEN rule engine
- delayed actions
- local history
- store-and-forward / retransmission
- historical data retrieval
- remote configuration
- time / timezone / DST support
- backup and restore
- watchdog
- firmware update / OTA concept

In addition, this project adds Modbus slave operation, Wi-Fi Web UI, Bluetooth LE, optional GNSS and the optional Victron compatibility component.

## Physical connectors

Target product interface:

- **4-pole terminal block:** `V+ / V- / A / B`
  - external supply: nominal target **5-30 V DC**
  - RS485 / Modbus A/B
- **RJ45:** Victron-compatible VE.Bus, optional
  - VE.Bus data
  - optional power source from VE.Bus V+/GND
- **USB-C:** firmware/service and, when Victron is enabled, experimental MK3-USB compatibility
- **Wi-Fi/BLE/LoRa/GNSS:** wireless or board-integrated interfaces

External supply and VE.Bus-derived supply must be isolated/ORed so they cannot back-feed each other.

## Configuration

The primary administration interface is a local Web UI over Wi-Fi.

- client mode: device joins an existing WLAN and displays IP/hostname
- AP commissioning mode: display shows SSID, AP password and configuration address
- initial administrator password is generated once and shown on the OLED
- first login forces an administrator password change
- long deliberate user-button action performs a complete factory reset
- configuration backup can be downloaded and later uploaded/restored through the Web UI

## Platform services

Board resources are exposed as shared platform services rather than being tied to one protocol component:

- OLED display
- user button
- status/user LEDs
- BAT connection and battery-voltage monitoring
- SOL connection / solar charging path where supported by the board
- switchable Vext
- free GPIO / ADC / PWM / touch
- I2C / SPI / UART expansion
- Wi-Fi
- Bluetooth LE
- native USB
- persistent storage
- watchdog
- OTA/update infrastructure

## Documentation

- `docs/specification.md` - consolidated product requirements
- `docs/architecture.md` - component and API architecture
- `docs/hardware.md` - reference hardware, connectors and electrical domains
- `docs/uc100-compatibility.md` - UC100 replacement feature scope
- `docs/rule-engine.md` - cross-component automation model
- `docs/web-ui.md` - WLAN, Web UI and local UI behaviour
- `docs/security-and-provisioning.md` - first setup, credentials, factory reset and backup/restore
- `docs/validation.md` - technical items that still require measurement or reverse engineering
- `docs/backlog.md` - later ideas such as Meshtastic

## Status

Specification and early firmware scaffold. Electrical interfaces, exact HTIT-WB32LAF V4.2 GPIO allocation, VE.Bus supply capability and Victron interoperability must be validated before connecting experimental hardware to a live inverter/charger installation.
