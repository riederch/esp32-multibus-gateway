# MultiBus Gateway V1 Specification

## Goal

Build a modular ESP32-S3 based multi-purpose fieldbus and LoRa platform on the Heltec HTIT-WB32LAF V4.2 reference board.

The platform is composed of independent functional components around a shared Core. Victron is optional and must not define the overall architecture.

## Component model

```text
V = [1,0]
L = [W,M,0]
M = [M,S,0]
G = [1,0]
```

Meaning:

```text
V=1  complete Victron cocoon enabled
V=0  Victron disabled

L=W  LoRaWAN
L=M  Meshtastic [backlog]
L=0  LoRa disabled

M=M  Modbus RTU master
M=S  Modbus RTU slave
M=0  Modbus disabled

G=1  GNSS enabled
G=0  GNSS disabled
```

LoRaWAN and Meshtastic are mutually exclusive because they share the SX1262.

## Reference hardware

- Heltec HTIT-WB32LAF V4.2 / WiFi LoRa 32 V4.2
- ESP32-S3
- SX1262 EU868
- onboard OLED
- onboard user button / LEDs
- native USB-C
- Wi-Fi / BLE
- BAT / SOL facilities of the module
- optional external GNSS module
- external isolated RS485 transceiver for Modbus
- external isolated RS485 transceiver for VE.Bus when Victron is used

## External connectors

### Field terminal

```text
V+  external DC supply input
V-  external DC supply return
A   RS485 / Modbus A
B   RS485 / Modbus B
```

Nominal target supply range: **5-30 V DC**.

### Victron RJ45

Optional Victron-compatible VE.Bus connector. It carries VE.Bus data and may also supply the gateway through VE.Bus V+/GND when validated.

## Power

The gateway must support either:

- external 5-30 V supply on V+/V-, or
- supply from VE.Bus V+/GND.

The two inputs must be protected and power-ORed/isolated so they cannot back-feed each other.

## Core services

The Core owns:

- configuration persistence
- capability registry
- event/property bus
- generic rule engine
- alarms
- local history
- store-and-forward / retransmission
- time / timezone / DST
- Wi-Fi
- Web UI/backend
- authentication/sessions
- backup/restore
- display/button/LED services
- board I/O abstraction
- watchdog
- firmware update / OTA infrastructure

## Victron component

When `V=1`, the Victron block provides the full Victron-specific experience:

- direct VE.Bus communication
- telemetry and settings access
- controlled settings writes
- optional STB / Panel Detect support
- VictronConnect BLE / Smart-Dongle compatibility research
- MK2/MK3 protocol engine
- native USB MK3-USB compatibility research

The rest of the platform accesses Victron only through defined properties, events and commands.

## LoRa component

### LoRaWAN (`L=W`)

Target V1 capabilities:

- EU868
- OTAA and ABP where supported by the stack
- Class A and C; Class C preferred for stationary externally powered use cases
- periodic telemetry uplinks
- event/alarm uplinks
- downlink commands
- remote configuration
- store-and-forward integration
- retransmission
- time synchronization where supported
- firmware update strategy / FUOTA research

### Meshtastic (`L=M`)

Backlog only. Intended later as an alternative local/mesh transport and conceptual replacement for Milesight D2D use cases. No Milesight D2D protocol compatibility is planned.

## Modbus component

### Master (`M=M`)

- configurable baud/parity/stop bits
- multiple slave devices
- configurable channels
- polling intervals
- read/write
- raw/transparent pass-through capability
- data type conversion
- byte/word order conversion
- scaling and offsets

### Slave (`M=S`)

- configurable slave ID
- virtual register map
- mapping of internal properties to registers/coils where suitable
- controlled writable mappings to internal commands

## GNSS component

When `G=1`, expose normalized GNSS properties:

- fix state
- latitude/longitude
- altitude
- UTC time
- satellites
- accuracy/HDOP where available
- speed/course
- PPS where available

## UC100 replacement scope

The project should reproduce the useful Milesight UC100 feature set except Milesight D2D:

- Modbus channel configuration
- periodic polling
- datatype/endian/scaling conversion
- transparent/raw RS485
- threshold alarms
- change alarms
- IF/THEN automation
- delayed actions
- local history
- store-and-forward
- retransmission
- historical data retrieval
- remote configuration
- time / timezone / DST
- configuration import/export through backup/restore
- watchdog
- firmware update / OTA concept

The project additionally provides Modbus slave mode, Wi-Fi/WebUI, BLE, GNSS and optional Victron functionality.

## Wi-Fi / Web UI

Primary device administration is through Wi-Fi and Web UI.

### Client mode

- join configured WLAN
- show connection state, IP and mDNS hostname on OLED

### AP commissioning mode

- provide own WLAN
- generate per-device random AP password
- display SSID, AP password and configuration IP on OLED
- optional captive portal

Use standard HTTP/HTTPS port where practical so no explicit port number is needed.

## Authentication

On first boot:

- generate AP password
- generate one-time initial administrator password
- show both on OLED

First administrator login must force password change. The initial password is then invalidated permanently and no longer displayed.

No universal/default or hidden master password is permitted.

## Physical reset

There is no button action for administrator-password-only reset.

A deliberate long user-button action performs a **complete factory reset** and returns the device to first-setup state.

## Backup / restore

Authenticated Web UI/backend functions must support:

- backup download
- backup upload/restore
- schema-versioned configuration
- validation before apply
- transactional/atomic restore
- optional encryption for backups containing secrets

Administrator password hashes and active sessions are not portable configuration and are not restored.

## Display

The OLED is a first-class local UI. Planned pages:

- system / firmware / uptime
- WLAN / IP / hostname
- LoRaWAN status
- Modbus status
- Victron status when enabled
- GNSS status when enabled
- power/BAT/SOL status
- alarms and diagnostic errors

Short button presses may cycle display pages.

## Platform I/O

Expose useful board resources through a generic platform abstraction:

- BAT / battery voltage where supported
- SOL / charging state where supported
- switchable Vext where supported
- user LED(s)
- free GPIO
- ADC
- PWM
- touch
- I2C
- SPI
- spare UARTs

Exact usable pins remain subject to V4.2 pin/resource validation.

## Principal use cases

```text
V0 / LW / MM  UC100-like Modbus master -> LoRaWAN
V0 / LW / MS  LoRaWAN <-> Modbus slave bridge
V1 / LW / MM  Victron + Modbus -> LoRaWAN
V1 / LW / M0  Victron -> LoRaWAN
V1 / L0 / M0  standalone Victron cocoon / compatibility adapter
V0 / L0 / MM  local Modbus gateway/logger
G1            GNSS augments any compatible configuration
LM            future Meshtastic variants
```

## V1 validation blockers

These are implementation validation tasks, not architecture questions:

- exact free GPIO allocation on HTIT-WB32LAF V4.2
- exact ordered GNSS module and pin capabilities
- VE.Bus V+ voltage/current budget and off/standby behaviour
- suitable VE.Bus RS485 timing / DE-RE behaviour
- native USB compatibility with VictronConnect / MK3 expectations
- Victron BLE Smart-Dongle compatibility behaviour
- final safe power-source ORing/isolation topology
- safe interaction of external supply, USB, BAT, SOL and VE.Bus-derived supply
