# Rule Engine

## Principle

Automation is a Core service. Rules consume normalized channels/events and invoke validated commands through public component APIs.

```text
Trigger -> Conditions -> Actions
```

## Trigger sources

- time/schedule
- device boot/restart
- channel value/change/error
- LoRaWAN downlink / remote command
- raw RS485 receive pattern when enabled
- Victron events/properties
- GNSS movement/fix/geofence events
- Core/system events
- Meshtastic messages when the Meshtastic backend is enabled

## Conditions

- equals / not equals
- greater / greater-or-equal
- less / less-or-equal
- inside/outside range
- changed by absolute delta
- true / false
- online / offline
- string/message match
- duration / hold time
- debounce / lockout

## Actions

- LoRaWAN telemetry/event/alarm uplink
- immediate channel report
- write normalized writable point
- Modbus write
- raw RS485 transmit
- Victron setting write through a whitelisted source point
- set internal/user variable
- create/clear alarm
- reboot
- delayed execution
- Meshtastic transmit when available

## Examples

```text
IF channel.pressure > 4.5
THEN alarm("high_pressure")
```

```text
IF victron:vebus/battery.voltage < 11.5
THEN lorawan.report("battery_low")
```

```text
IF remote.command == "pump_stop"
THEN write(modbus:12/pump.run, false)
```

```text
IF time == 12:00
THEN write(victron:vebus/charger.current, 5.0)
```

```text
IF gnss:primary/distance.home > 500
THEN alarm("geofence")
```

## Local execution

Rules execute locally without Wi-Fi or LoRaWAN connectivity. Only actions that explicitly use a remote transport depend on connectivity.

## Safety

Writable actions require:

- source capability checks
- writable-point allowlists
- datatype/range validation
- component-specific safety validation

Remote payloads must never become unrestricted memory, register or command execution paths.

## Persistence

Rules are persistent configuration and are included in backup/restore and supported remote-configuration mechanisms.

## Capacity

Rule count and action count are determined by available memory/storage and reported by the firmware; no arbitrary low fixed protocol limit is imposed internally.
