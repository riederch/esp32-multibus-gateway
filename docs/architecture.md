# Architecture

## Overview

MultiBus Gateway uses a shared Core with independent protocol and hardware components. Components connect to the Core through explicit roles: **data sources**, **consumers** and **transports**.

```text
                         +---------------------------+
                         |           CORE            |
                         | source registry           |
                         | channel registry          |
                         | event / command bus       |
                         | rules / history / alarms  |
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

LoRa is a peer component connected directly to the Core. It is not attached to Victron, VE.Bus or any other data source.

The Core does not depend on Modbus, VE.Bus, GNSS or LoRaWAN wire formats. Native protocol adapters translate between their wire protocol and normalized Core objects.

## Component state model

```text
V=1  Victron enabled
V=0  Victron disabled

L=W  LoRaWAN
L=M  Meshtastic
L=0  LoRa disabled

M=M  Modbus RTU master
M=S  Modbus RTU slave
M=0  Modbus disabled

G=1  GNSS enabled
G=0  GNSS disabled
```

`L=W` and `L=M` are mutually exclusive because they share the SX1262 radio.

## Roles

### Data source

A data source exposes normalized points to the Core.

Examples:

```text
modbus:12
victron:vebus
gnss:primary
platform:power
```

A source exposes:

- identity
- online/health state
- readable points
- writable points where permitted
- point metadata
- source-specific events

### Consumer

Consumers use normalized points, channels and events without parsing native wire protocols.

Examples:

- rule engine
- history
- alarms
- Web UI
- diagnostics

### Transport

A transport carries Core data and commands to or from an external system.

Examples:

- LoRaWAN
- Meshtastic
- future MQTT or other IP transports

A transport does not own the source values it carries. It encodes outbound Core objects and decodes inbound remote messages into validated Core commands/events.

## Data-source abstraction

Example points:

```text
modbus:12/pressure.bar
modbus:12/flow.m3h
victron:vebus/battery.voltage
victron:vebus/charger.current
victron:vebus/ac.input.voltage
gnss:primary/position.latitude
platform:power/battery.voltage
```

Victron is not represented internally as a fake Modbus slave. Both Modbus and Victron implement the same source abstraction above their native transport.

## Channel model

A channel binds one normalized source point to reporting, alarm, history and remote-control behaviour.

Conceptual definition:

```text
channel
  id
  enabled
  source
  point
  datatype
  scale
  offset
  unit
  poll/report policy
  alarm policy
  history policy
  writable
```

For Modbus sources, transport-specific fields belong to the source binding:

```text
slave_id
function
address
register_count
endianness
signedness
poll_interval
```

For Victron sources, the binding references a Victron property instead of inventing a Modbus address.

This separation means that LoRaWAN, rules, history and the Web UI do not need to know where a value originated.

## LoRa transport

The LoRa component owns the onboard SX1262 and implements one active backend at a time.

### LoRaWAN (`L=W`)

LoRaWAN transports:

- channel telemetry
- alarms/events
- history/retransmission data
- remote configuration
- validated commands

Inbound flow:

```text
LoRaWAN downlink
    -> LoRa transport
    -> protocol decoder
    -> validated Core command/event
    -> target source/service
```

Outbound flow:

```text
DataSource
    -> normalized point
    -> channel/event/history
    -> LoRaWAN encoder
    -> LoRa transport
    -> SX1262
```

### Meshtastic (`L=M`)

Meshtastic is an alternative transport backend for the same Core channel/event/command model. It is mutually exclusive with LoRaWAN on the onboard SX1262.

## LoRaWAN compatibility channels

The FPort-85 compatibility profile uses logical channels compatible with the Milesight UC100 V2 wire protocol.

For Modbus-backed channels, the mapping is direct. For Victron-, GNSS- or platform-backed channels, MultiBus binds the native point to a logical compatibility channel and then uses the same telemetry/alarm/history encoder.

Creating or changing a non-Modbus source binding uses the MultiBus extension protocol or Web UI because the compatibility command set has no representation for these native sources.

## Commands

Commands are normalized in the Core and executed only through explicit source/service capabilities.

Examples:

```text
write(modbus:12/setpoint, 35)
write(victron:vebus/charger.current, 5.0)
invoke(system/reboot)
invoke(lorawan/rejoin)
```

The command layer enforces:

- capability checks
- type/range validation
- writable allowlists
- source-specific safety rules

## Victron component

The Victron component owns all Victron-specific interoperability:

```text
Victron component
|- VE.Bus transport/timing/frame handling
|- telemetry and settings
|- controlled writes
|- Standby / Panel Detect support where implemented
|- MK2/MK3 protocol engine
|- USB MK3 compatibility layer
`- VictronConnect BLE compatibility layer
```

Its public data interface is the generic source/channel API plus explicitly exposed Victron capabilities. No other component parses VE.Bus frames.

## Modbus component

### Master mode

- RS485 Modbus RTU master
- configurable serial parameters
- multiple slave devices
- register/channel polling
- coils, discrete inputs, input registers and holding registers
- supported write operations
- type/endian/scaling conversion
- raw/transparent RS485 access

### Slave mode

- configurable slave ID
- virtual register/coils map
- normalized Core points may be exposed as Modbus objects
- writable mappings invoke only explicitly allowed Core/source commands

## GNSS component

GNSS exposes normalized properties including:

- fix state
- latitude / longitude
- altitude
- UTC time
- satellites
- accuracy/HDOP
- speed / course
- PPS state where available

Raw NMEA remains inside the GNSS component.

## Rule engine

Rules operate only on normalized points/events/commands.

Examples:

```text
IF channel.pressure > 4.5
THEN alarm("high_pressure")

IF victron:vebus/battery.voltage < 11.5
THEN report(channel.battery_voltage)

IF remote.command == "pump_stop"
THEN write(modbus:12/pump.run, false)
```

Rules do not call LoRaWAN or VE.Bus frame handlers directly.

## Core services

The Core owns:

- persistent configuration
- data-source registry
- transport registry
- channel registry
- capability registry
- event/command bus
- rule engine
- alarms
- local history
- store-and-forward/retransmission state
- time/timezone/DST
- Wi-Fi and Web UI
- authentication and sessions
- backup/restore
- board UI and I/O abstraction
- watchdog
- firmware update/OTA infrastructure

## Platform services

```text
Platform
|- Power
|  |- external supply state
|  |- BAT / battery voltage
|  |- SOL / charge state where available
|  `- Vext control
|- UI
|  |- OLED
|  |- user button
|  `- LED
|- Connectivity
|  |- Wi-Fi
|  |- BLE
|  `- native USB
|- Expansion
|  |- GPIO / ADC / PWM / touch
|  |- I2C / SPI
|  `- available UARTs
`- System
   |- storage
   |- watchdog
   |- time
   `- update services
```

## Electrical separation

VE.Bus and Modbus use separate RS485 transceiver paths. The design must prevent USB, VE.Bus and field wiring from creating unintended ground paths. External supply and VE.Bus-derived supply must not back-feed each other.
