# Architecture

## Product model

The MultiBus Gateway is a modular platform built from independent functional components around a shared core.

```text
                            +----------------------+
                            |        CORE          |
                            | config / rules / UI  |
                            | history / events     |
                            | security / updates   |
                            +----------+-----------+
                                       |
                 +---------------------+---------------------+
                 |                     |                     |
          +------v------+       +------v------+       +------v------+
          |   Victron   |       |     LoRa    |       |   Modbus    |
          |   V=[1,0]   |       | L=[W,M,0]   |       | M=[M,S,0]   |
          +------+------+       +------+------+       +------+------+ 
                 |                     |                     |
             VE.Bus               SX1262 radio          RS485 port
                 |
                 +-----------------------------------------------+
                                       |
                                +------v------+
                                |    GNSS     |
                                |   G=[1,0]   |
                                +-------------+
```

The components are separately configurable and may be disabled independently, subject only to physical resource conflicts.

## Component state model

### Victron

```text
V=1  complete Victron compatibility component enabled
V=0  Victron component disabled
```

### LoRa

```text
L=W  LoRaWAN backend
L=M  Meshtastic backend [backlog / not implemented]
L=0  LoRa radio disabled
```

`L=W` and `L=M` are mutually exclusive because they share the SX1262 radio.

### Modbus

```text
M=M  Modbus RTU master
M=S  Modbus RTU slave
M=0  Modbus disabled
```

Transparent/raw RS485 is treated as a capability of the Modbus/RS485 component rather than a separate top-level state.

### GNSS

```text
G=1  GNSS enabled
G=0  GNSS disabled
```

The exact external GNSS module type is not yet fixed in the specification and must be identified/validated from the ordered hardware.

## Core responsibilities

The Core owns all cross-component functionality:

- configuration and persistence
- capability registry
- event bus / internal property model
- rule engine
- alarms
- local history
- store-and-forward / retransmission state
- time, timezone and DST
- Wi-Fi and Web UI
- authentication and sessions
- backup/restore
- firmware/update infrastructure
- display and button services
- board I/O abstraction
- diagnostics and watchdog

Protocol-specific logic must not leak into the Core.

## Capability registry

Each component publishes its active capabilities to the Core.

Example:

```text
victron:
  online
  battery_voltage
  charger_current
  charger_current_set
  ac_input
  mk3_usb
  victron_ble

lorawan:
  joined
  uplink
  downlink
  remote_config

modbus:
  mode=master
  read
  write
  raw_passthrough

gnss:
  fix
  position
  speed
  course
  utc_time
```

Web UI, rules and other components consume capabilities rather than depending on implementation details.

## Victron cocoon

The Victron component is intentionally a closed block. When `V=1`, it owns the full Victron-specific feature set:

```text
+------------------------------------------------------+
| Victron component                                    |
|                                                      |
| VE.Bus transport / timing / frame handling           |
| settings + telemetry                                 |
| controlled writes                                    |
| Standby / Panel Detect support when implemented      |
| MK2/MK3 protocol engine                              |
| USB MK3 compatibility layer                          |
| VictronConnect BLE / Smart-Dongle compatibility      |
+---------------------------+--------------------------+
                            |
                    defined public API
                            |
             properties / events / commands
```

Other components must never parse VE.Bus frames or know Victron-specific protocol state.

Example public operations:

```text
victron.is_online
victron.battery_voltage
victron.ac_input_voltage
victron.charge_current
victron.state
victron.set_charge_current(...)
```

The first VE.Bus target is the Victron MultiPlus 12/500/20-16.

## LoRa component

The LoRa component owns the SX1262 radio and exposes a transport-neutral interface to the Core.

### LoRaWAN (`L=W`)

V1 target capabilities:

- OTAA and ABP as supported by the selected stack
- EU868
- Class A/C support, with Class C preferred for mains-powered/stationary deployments
- periodic uplinks
- event/alarm uplinks
- downlink commands
- remote configuration
- retransmission/store-and-forward integration
- time synchronization where supported
- OTA/FUOTA strategy

### Meshtastic (`L=M`)

Reserved for later implementation. It is intended to replace the use cases that Milesight D2D would otherwise cover, not to emulate Milesight D2D protocol compatibility.

## Modbus component

The Modbus/RS485 component supports three operational capability sets under two main states.

### Master (`M=M`)

- configurable baud/parity/stop bits
- multiple slave devices
- configurable channels/register definitions
- periodic polling
- read/write operations
- raw/transparent passthrough when enabled

### Slave (`M=S`)

- configurable slave ID
- virtual register map
- internal properties can be mapped to holding/input registers, coils or discrete inputs where suitable
- writable mappings may invoke controlled Core/component commands

### Disabled (`M=0`)

RS485 protocol processing is stopped and related resources may be powered down where practical.

## GNSS component

The GNSS component exposes normalized data rather than raw NMEA to the rest of the system.

Target properties:

- fix state
- latitude
- longitude
- altitude
- UTC time
- satellites
- HDOP/accuracy where available
- speed
- course
- PPS where supported

Possible consumers include LoRaWAN telemetry, history, time synchronization, display pages and automation/geofencing.

## Rule engine integration

The rule engine belongs to the Core and uses only public component properties/events/commands.

Examples:

```text
IF modbus.channel.pressure > 4.5
THEN lorawan.send_alarm(...)

IF victron.battery_voltage < 11.5
THEN lorawan.send_alarm(...)

IF lorawan.command == "pump_stop"
THEN modbus.write(...)

IF time == 12:00
THEN victron.set_charge_current(...)
```

## Platform services

Board resources are exposed through a hardware/platform abstraction layer:

```text
Platform
|- Power
|  |- external input
|  |- optional VE.Bus-derived input
|  |- BAT
|  |- SOL
|  |- battery measurement
|  `- Vext control
|- UI
|  |- OLED
|  |- user button
|  `- LEDs
|- Connectivity
|  |- Wi-Fi
|  |- Bluetooth LE
|  `- native USB
|- Expansion I/O
|  |- GPIO
|  |- ADC
|  |- PWM
|  |- touch
|  |- I2C
|  |- SPI
|  `- spare UART where available
`- System
   |- persistent storage
   |- watchdog
   |- time
   `- update infrastructure
```

These platform services are not themselves Victron, LoRaWAN, Modbus or GNSS components.

## Wi-Fi and Web administration

Wi-Fi/WebUI is the primary platform administration interface.

- client mode: connect to configured WLAN and show IP/mDNS name on OLED
- AP commissioning mode: display SSID, AP password and configuration IP
- optional captive portal
- standard HTTP/HTTPS port preferred to avoid displaying an extra port
- BLE is not required for normal platform administration

When `V=1`, BLE and USB may additionally expose Victron-specific compatibility functions inside the Victron cocoon.

## USB architecture

Native ESP32-S3 USB is a platform service. Generic functions include firmware/service/diagnostics.

When `V=1`, the Victron component may additionally expose an MK2/MK3-compatible USB transport. The MK2/MK3 protocol engine must remain independent of the concrete USB backend.

Preferred path:

1. test native ESP32-S3 USB device emulation
2. emulate required FTDI-like control behaviour only where technically/legal appropriate
3. if host compatibility requires it, use a genuine FTDI device on a later PCB as a fallback

## Power-source independence

Victron is optional, therefore the platform cannot depend on VE.Bus for power.

Supported source concepts:

- external `V+ / V-` on the field terminal, nominal target 5-30 V DC
- VE.Bus V+ / GND when a Victron device is connected and the available supply has been validated

Both sources must be protected and ORed/isolated so they cannot back-feed each other.

## Principal use cases

```text
V0 / LW / MM  UC100-like Modbus -> LoRaWAN gateway
V0 / LW / MS  LoRaWAN <-> Modbus slave bridge
V1 / LW / MM  Victron + Modbus -> LoRaWAN
V1 / LW / M0  Victron -> LoRaWAN
V1 / L0 / M0  standalone Victron compatibility adapter
V0 / L0 / MM  local Modbus gateway/logger
G1            adds GNSS capabilities to any compatible mode
LM            later Meshtastic variants of the above
```

## Safety boundaries

- VE.Bus and Modbus use independent RS485 interfaces.
- Maintain galvanic isolation so USB/PC, VE.Bus and external field wiring do not create unintended ground paths.
- Do not assume automatic-direction RS485 is suitable for production VE.Bus timing until measured.
- Use explicit DE/RE control for final VE.Bus hardware.
- Treat direct VE.Bus control, MK3 emulation and Victron BLE emulation as experimental until validated.
