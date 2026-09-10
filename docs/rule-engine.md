# Rule Engine

## Principle

Automation is a Core feature. It must not belong to Modbus, LoRaWAN, Victron or GNSS individually.

Rules consume normalized properties/events and invoke defined component commands through public APIs.

## Model

```text
Trigger
  -> Conditions
  -> Actions
```

A rule may contain one or more conditions and one or more actions. Actions may optionally be delayed.

## Trigger sources

V1 target trigger classes:

- time/schedule
- device boot/restart
- Modbus channel value/change/error
- LoRaWAN downlink / remote command
- raw RS485 receive pattern when enabled
- Victron property/event when `V=1`
- GNSS property/event when `G=1`
- Core/system events
- later Meshtastic event/message when `L=M`

## Conditions

Target operators:

- equals / not equals
- greater / greater-or-equal
- less / less-or-equal
- inside/outside range
- changed by absolute delta
- true / false
- online / offline
- string/message match where appropriate
- duration/hold time
- debounce/lockout

## Actions

Target action classes:

- LoRaWAN telemetry/event/alarm uplink
- immediate data upload
- Modbus write
- raw RS485 transmit
- set internal/user variable
- create/clear alarm
- write a whitelisted Victron setting when `V=1`
- request component operation through its public API
- reboot device
- delay action execution
- later send Meshtastic message

## Examples

```text
IF modbus.pressure > 4.5
THEN lorawan.send_alarm("high_pressure")
```

```text
IF victron.battery_voltage < 11.5
THEN lorawan.send_alarm("battery_low")
```

```text
IF lorawan.command == "pump_stop"
THEN modbus.write(pump.stop)
```

```text
IF time == 12:00
THEN victron.set_charge_current(5A)
```

```text
IF gnss.distance_from(home) > 500m
THEN lorawan.send_alarm("geofence")
```

## Local operation

Rules must execute locally even when LoRaWAN/Wi-Fi is unavailable. Remote connectivity is only required for actions that explicitly need it.

## Safety

Writable actions must be capability-checked and whitelisted. Generic remote payloads must never become unrestricted memory/register/command execution paths.

## Persistence

Rules are persistent configuration and therefore included in backup/restore and remote configuration mechanisms.

## Limits

Do not copy the UC100's fixed 16-rule limit unless required by resources. Use implementation-defined limits with clear reporting in the Web UI/API.
