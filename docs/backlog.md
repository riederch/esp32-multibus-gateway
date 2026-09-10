# Backlog

This file contains explicitly deferred ideas that are useful to preserve in the architecture but are not required for V1.

## Meshtastic backend (`L=M`)

Status: idea / architecture placeholder only.

Goal:

- use the onboard SX1262 as an alternative to LoRaWAN
- provide direct/mesh node communication without requiring Milesight D2D
- reuse the same Core event/property/action model as LoRaWAN
- allow future Modbus/Victron/GNSS data and commands to be transported over Meshtastic

Constraints:

- mutually exclusive with LoRaWAN on the same SX1262
- licensing implications must be reviewed before integrating Meshtastic firmware/code
- do not couple Core or component APIs to Meshtastic-specific packet formats

Possible future examples:

```text
IF modbus.pressure > threshold
THEN meshtastic.send(...)

IF meshtastic.command == "pump_stop"
THEN modbus.write(...)

IF victron.battery_voltage < threshold
THEN meshtastic.send(...)
```

## Victron BLE compatibility

- emulate enough VE.Bus Smart Dongle behaviour for VictronConnect where feasible
- keep fully inside the Victron cocoon
- do not make platform administration depend on this feature

## Native USB MK3 compatibility

- emulate MK3-facing USB transport with native ESP32-S3 USB if practical
- support VictronConnect without external MK3 hardware where feasible
- retain genuine FTDI hardware as a future compatibility fallback

## MQTT / Wi-Fi integrations

- MQTT publish/subscribe
- expose normalized component properties/events
- remote commands through whitelisted Core APIs
- optional Home Assistant discovery later

## Advanced GNSS functions

- geofencing
- movement-dependent reporting intervals
- GNSS-based commissioning location
- GNSS time/PPS as high-quality system clock source

## Additional platform I/O

Possible generic adapters/features:

- pulse counters
- digital inputs
- relay/open-drain outputs
- 0-10 V adapters
- 4-20 mA adapters
- 1-Wire sensors
- I2C environmental sensors

All should use the generic Platform/Capability model rather than application-specific hardcoding.
