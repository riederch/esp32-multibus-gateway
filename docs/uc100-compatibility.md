# UC100 compatibility target

## Goal

Provide a practical replacement for the useful Milesight UC100 feature set while remaining a broader multi-purpose platform.

Milesight D2D is explicitly excluded. Future mesh/direct-radio use cases belong to the Meshtastic backlog instead.

## Modbus RTU

Target capabilities:

- RS485 physical interface
- configurable baud rate
- configurable parity
- configurable stop bits
- Modbus RTU master mode
- multiple slave devices
- configurable channel/register definitions
- read coils
- read discrete inputs
- read input registers
- read holding registers
- write supported writable objects/registers
- configurable polling intervals
- retry count
- response timeout
- optional selectable 120 ohm termination

## Channel model

Each configured channel should support at least:

- unique ID
- name
- enabled state
- slave ID
- function/object type
- start address
- register count
- datatype
- signed/unsigned interpretation
- byte/word order
- scale/multiplicator
- offset
- unit
- polling interval
- deadband/change threshold where applicable

Target numeric types include at least:

- boolean/coil
- INT16 / UINT16
- INT32 / UINT32
- FLOAT32
- INT64 / UINT64 where practical
- FLOAT64 / DOUBLE64 where practical

Support common byte/word orders such as ABCD, BADC, CDAB and DCBA where relevant.

Do not impose the UC100's channel/register limits unless required by available resources. Configuration limits should be implementation-defined and documented.

## Modbus slave extension

This project additionally supports Modbus RTU slave mode.

Target capabilities:

- configurable slave ID
- virtual register map
- expose internal platform/component values as registers/coils
- writable mappings may call explicitly whitelisted commands
- no arbitrary unsafe command execution through writable registers

## Transparent/raw RS485

Support a transparent/raw path for advanced use cases:

- send raw request bytes to RS485
- capture response
- transport request/response via supported remote interfaces
- optional two-way/pass-through mode where technically appropriate

Raw access is a capability of the RS485/Modbus component, not a separate top-level M state.

## Periodic data collection

- independently configurable channel polling
- periodic reporting/upload interval
- collection error reporting
- timestamped values
- batch multiple channel values efficiently

## Threshold alarms

Support configurable alarms for numeric values:

- above
- below
- within/outside range
- optional hold duration
- lockout/debounce period
- alarm text/identifier
- alarm release event

For boolean values support true/false transitions.

## Change alarms

Support alarms based on value change:

- absolute delta threshold
- optional time window
- optional percentage delta later
- include current value and change value in event payload where useful

## Rule engine

The generic Core rule engine replaces UC100 IF/THEN automation while supporting all components.

Target trigger classes include:

- scheduled/local-time trigger
- Modbus channel condition/change
- LoRaWAN downlink / remote command
- raw RS485 receive pattern where enabled
- device boot/restart
- Victron property/event when V=1
- GNSS property/event when G=1
- later Meshtastic message/event

Target action classes include:

- send LoRaWAN telemetry/event/alarm
- execute a Modbus write
- send raw RS485 data
- request immediate data upload
- create alarm/release event
- change a whitelisted Victron setting when V=1
- set internal/user variable
- reboot device
- delayed execution

Rules must continue to work locally without LoRaWAN connectivity.

## Local history

Persist timestamped operational data in flash using a bounded/ring-buffer strategy.

History should support:

- normal measurements
- communication failures
- alarm events and releases
- rule-generated events
- remote/user messages where relevant

Do not copy the UC100's small history limit; use an implementation-defined flash budget.

## Store and forward

When the remote LoRaWAN path is unavailable:

- continue polling/collecting locally
- persist eligible records
- track pending upload state
- after reconnection send current data and then historical backlog according to policy
- avoid duplicate replay where practical

## Retransmission

Support configurable retransmission for pending history/events:

- enable/disable
- retry/retransmission interval
- max retry policy where needed
- acknowledgement semantics where supported by the transport

## Historical data retrieval

Remote or Web UI requests should be able to retrieve history by:

- specific timestamp/range
- recent N records
- channel/filter where useful
- stop/cancel transfer
- clear history through an authenticated/destructive operation

## Remote configuration

LoRaWAN downlink or another authenticated remote-management path should support controlled changes to:

- reporting intervals
- Modbus serial parameters
- Modbus channels
- rules
- alarms
- retransmission/store-and-forward settings
- time/timezone/DST
- component settings where safe
- reboot/rejoin operations

Configuration changes must be validated before persistence.

## Time

Support:

- UTC time
- configurable timezone
- DST rules or timezone handling suitable for local schedules
- time synchronization from available sources such as GNSS, LoRaWAN/network, browser/Web UI or NTP when Wi-Fi is available

## Backup / restore

Replace simple UC100 configuration export/import with full platform backup/restore as defined in `security-and-provisioning.md`.

## Firmware and watchdog

- embedded watchdog
- local firmware update
- Wi-Fi OTA update
- recovery via USB
- investigate LoRaWAN FUOTA feasibility

## Explicitly not copied

### Milesight D2D

Not implemented and not required for UC100 replacement target.

Future direct/mesh node communication is planned through the Meshtastic backend (`L=M`) using the platform's generic event/action APIs rather than emulating Milesight D2D.
