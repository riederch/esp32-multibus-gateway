# Architecture

## Overview

MultiBus Gateway uses a shared Core with independent protocol and hardware components.

```text
+--------------------------------------------------------------+
|                            Core                              |
| config | channels | rules | history | security | Web UI     |
| events | alarms   | time  | backup  | updates  | diagnostics|
+-----------------------------+--------------------------------+
                              |
                +-------------+-------------+
                |             |             |
             Modbus        Victron         GNSS
             source         source          source
                |             |             |
              RS485         VE.Bus         UART/PPS
                \             |             /
                 +------------+------------+
                              |
                         Channel model
                              |
          +-------------------+-------------------+
          |                   |                   |
       LoRaWAN              Rules              History
```

The Core does not depend on Modbus, VE.Bus or GNSS wire formats. Each component translates its native protocol into normalized channels, events and commands.

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

## Data-source abstraction

Modbus and Victron are deliberately unified above their transport layer.

A data source exposes:

- identity
- health/online state
- readable properties
- writable properties where permitted
- events
- commands/actions

Example source IDs:

```text
modbus:12
victron:vebus
platform:power
gnss:primary
```

A source may expose any number of normalized points.

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

For Modbus sources, transport-specific fields are attached to the source binding:

```text
slave_id
function
address
register_count
endianness
signedness
poll_interval
```

For Victron sources, the binding references a Victron property rather than fabricating a Modbus slave address/register.

This separation means that LoRaWAN, the rule engine, history and the Web UI do not need to know whether a value originated from a Modbus register or VE.Bus.

## Compatibility channels

The LoRaWAN FPort-85 compatibility profile uses the same logical channel representation as the Milesight UC100 V2 protocol.

For Modbus-backed channels the mapping is direct.

For Victron-backed channels the MultiBus firmware allocates/configures a compatibility channel and feeds its normalized value into the same reporting/alarm/history encoder. The uplink format therefore remains compatible even though the underlying source is VE.Bus.

Creating or changing a Victron binding is a MultiBus extension because the original compatibility protocol has no concept of a VE.Bus property. It must not redefine existing FPort-85 command identifiers.

## Commands

Commands are normalized in the Core and executed only through explicit source capabilities.

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

Its public interface is the generic source/channel API plus explicitly exposed Victron capabilities.

No other component parses VE.Bus frames.

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

Raw NMEA is kept inside the GNSS component.

## LoRa component

### LoRaWAN

LoRaWAN provides two protocol surfaces:

1. **FPort 85 compatibility profile** for the UC100 V2-compatible command and payload set.
2. **MultiBus extension FPort** for features that cannot be represented by the compatibility protocol.

The compatibility profile remains stable and extensions never reuse or reinterpret an existing compatibility command.

### Meshtastic

Meshtastic is an alternative transport backend for the same Core event/channel/command model. It is mutually exclusive with LoRaWAN on the onboard SX1262.

## Core services

The Core owns:

- persistent configuration
- data-source registry
- channel registry
- capability registry
- event bus
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

## Rule engine

Rules operate only on normalized points/events/commands.

Examples:

```text
IF channel.pressure > 4.5
THEN alarm("high_pressure")

IF victron:vebus/battery.voltage < 11.5
THEN lorawan.report("battery_low")

IF remote.command == "pump_stop"
THEN write(modbus:12/pump.run, false)
```

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
