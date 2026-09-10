# MultiBus Gateway Specification

## Purpose

MultiBus Gateway is an ESP32-S3 based field gateway combining LoRaWAN, RS485/Modbus, Victron VE.Bus, GNSS, local automation and board I/O behind a common channel/event/command model.

## Component modes

```text
V = [1,0]       Victron enabled / disabled
L = [W,M,0]     LoRaWAN / Meshtastic / disabled
M = [M,S,0]     Modbus RTU master / slave / disabled
G = [1,0]       GNSS enabled / disabled
```

LoRaWAN and Meshtastic are mutually exclusive because they use the same SX1262 radio.

## Reference hardware

- Heltec HTIT-WB32LAF V4.2 / WiFi LoRa 32 V4.2
- ESP32-S3
- SX1262 EU868
- OLED display
- Wi-Fi / BLE
- native USB-C
- BAT / SOL interfaces
- external GNSS interface
- isolated RS485 interface for Modbus
- isolated RS485 interface for VE.Bus

## External interfaces

### Field terminal

```text
V+  5-30 V DC supply
V-  supply return
A   Modbus RS485 A
B   Modbus RS485 B
```

### Victron RJ45

Optional VE.Bus interface carrying VE.Bus A/B and, where validated, an optional power source from VE.Bus V+/GND.

### USB-C

- flashing
- service / diagnostics
- recovery
- firmware update
- optional Victron MK2/MK3 compatibility transport

## Core services

The Core provides:

- configuration persistence
- data-source registry
- transport registry
- channel registry
- capability registry
- event/command bus
- rule engine
- alarms
- local history
- store-and-forward / retransmission
- time / timezone / DST
- Wi-Fi and Web UI
- authentication and sessions
- backup / restore
- display / button / LED services
- board I/O abstraction
- watchdog
- firmware update / OTA

## Source, channel and transport model

Modbus, Victron, GNSS and platform I/O expose normalized data points through a common source interface.

Examples:

```text
modbus:12/pressure.bar
modbus:12/flow.m3h
victron:vebus/battery.voltage
victron:vebus/charger.current
gnss:primary/position.latitude
platform:power/battery.voltage
```

A channel references one source point and adds reporting, scaling, alarm, history and write-policy metadata.

Transports carry channel/event/command data between the Core and external systems. LoRa is a transport component and is independent of Modbus, Victron and GNSS.

## Modbus

### Master mode

- configurable baud, parity and stop bits
- multiple slave devices
- coils, discrete inputs, input registers and holding registers
- supported write functions
- configurable polling
- retry and timeout policy
- INT16/UINT16, INT32/UINT32, FLOAT32, INT64/UINT64 and FLOAT64 where practical
- configurable byte/word order
- scale and offset
- raw/transparent RS485 access

### Slave mode

- configurable slave ID
- virtual register/coil map
- mappings from normalized channels to Modbus objects
- writable mappings only for explicitly allowed commands

## Victron

The Victron component owns:

- VE.Bus transport and frame handling
- telemetry and settings
- controlled writes
- STB / Panel Detect support where implemented
- MK2/MK3 protocol engine
- USB MK3 compatibility layer
- VictronConnect BLE compatibility layer

Victron values and settings are exposed as normalized source points. Victron is not represented as a synthetic Modbus slave.

## LoRa

The LoRa component owns the onboard SX1262 and acts as a Core transport.

### LoRaWAN

- EU868
- OTAA as primary activation
- LoRaWAN 1.0.x compatibility target, with 1.0.4 preferred for V1
- Class A and Class C
- periodic telemetry
- alarms and events
- downlink commands
- remote configuration
- history retrieval
- retransmission / store-and-forward
- time synchronization where supported
- firmware update strategy / FUOTA where feasible

The standard compatibility profile uses FPort 85 and is wire-compatible with the applicable non-D2D Milesight UC100 V2 protocol. MultiBus extensions use a separate configurable FPort and do not redefine compatibility commands.

### LoRaWAN identity

The device stores the LoRaWAN network identity independently of server-side ChirpStack objects.

- **DevEUI:** stable 64-bit per-device identity. Development/private deployments may derive it deterministically from the ESP32 factory identity. Production hardware must support provisioning an assigned EUI-64. Factory reset must not silently change a provisioned hardware identity.
- **JoinEUI:** configurable 64-bit value shared by the intended join/application domain; it is not generated independently for every device.
- **AppKey:** individual 128-bit cryptographically random OTAA root key. It is generated/provisioned per device and stored as a secret.
- **ChirpStack Application ID:** server-side metadata/UUID and not a LoRaWAN air-interface identifier. It is not required by the end device for OTAA.

The Web UI must allow authorized viewing/copying or provisioning of DevEUI/JoinEUI/AppKey as appropriate, without exposing secrets through unauthenticated diagnostics or logs.

### Meshtastic

Meshtastic is the alternative `L=M` transport backend and uses the same Core channel/event/command model. It does not emulate proprietary Milesight D2D.

## GNSS

Expose normalized:

- fix state
- latitude / longitude
- altitude
- UTC time
- satellites
- accuracy / HDOP
- speed / course
- PPS state where available

GNSS may provide position, movement triggers and system time.

## Automation

Rules use normalized channels, events and commands:

```text
Trigger -> Conditions -> Actions
```

Supported classes include:

- time schedules
- channel changes / thresholds
- communication errors
- remote transport commands
- RS485 receive patterns where enabled
- device boot/restart
- GNSS movement/geofence events
- delayed actions
- Modbus writes
- Victron writes through explicit capabilities
- telemetry/alarm upload
- user variables
- reboot

## History

Persist timestamped measurements, alarms, communication failures and rule events in a bounded flash-backed ring buffer with wear-aware storage.

History supports local viewing, remote retrieval and store-and-forward after connectivity loss.

## Wi-Fi and Web UI

### Client mode

Connect to a configured WLAN and expose the Web UI through IP address and mDNS hostname.

### AP mode

Provide a commissioning WLAN and display:

- SSID
- random AP password
- configuration address

Captive-portal support may redirect clients to the Web UI.

## Authentication

- random one-time administrator password at initial provisioning
- initial password displayed locally on OLED
- mandatory password change after first login
- no universal/default/master password
- salted password hash only
- login rate limiting
- session cookies

## Physical reset

A deliberate long user-button press performs a complete factory reset. There is no password-only physical reset.

## Backup / restore

The Web UI provides authenticated backup download and restore upload.

- versioned format
- full validation before apply
- atomic persistence
- administrator password and sessions excluded
- secret-bearing backups must support encryption
- portable LoRaWAN configuration may include JoinEUI/AppKey when explicitly included in an encrypted backup
- hardware-bound/provisioned DevEUI is not overwritten implicitly when restoring onto another device

## Firmware distribution and update

Successful `main` builds publish the application firmware binary and SHA-256 digest as GitHub Actions artifacts. Version tags publish a versioned GitHub Release asset.

The Web UI OTA path accepts an application `.bin`, validates it before activation where possible, writes it to the update partition and reboots. Signed-update and rollback policy are security-hardening items.

## Display

OLED pages cover:

- system / uptime / firmware
- WLAN / IP / hostname
- LoRaWAN state
- Modbus state
- Victron state
- GNSS state
- power / battery
- alarms / diagnostics

## Platform I/O

Expose supported board resources through the platform layer:

- battery voltage
- BAT / SOL state where detectable
- Vext control
- user LED
- GPIO
- ADC
- PWM
- touch
- I2C
- SPI
- available UARTs

## Power

Normal power sources:

- external 5-30 V DC field input
- optional VE.Bus-derived supply
- board BAT / SOL facilities as supported by the final electrical design

Power sources must not back-feed each other. USB connection must not defeat the intended bus isolation.
