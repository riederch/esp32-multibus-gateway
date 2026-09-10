# Backlog

Deferred optional features that are compatible with the current architecture.

## Meshtastic backend (`L=M`)

- use the onboard SX1262 as an alternative transport backend
- reuse the Core channel/event/command model
- allow Modbus, Victron, GNSS and platform data to be transported through the mesh
- remain mutually exclusive with LoRaWAN on the onboard radio
- review licensing implications before integrating third-party Meshtastic code

## MQTT

- publish normalized channels/events
- subscribe to whitelisted commands
- optional Home Assistant discovery

## Advanced GNSS

- geofencing
- movement-dependent reporting intervals
- GNSS-based commissioning location
- PPS-disciplined system time

## Additional platform I/O

- pulse counters
- digital inputs
- relay/open-drain outputs
- 0-10 V adapters
- 4-20 mA adapters
- 1-Wire sensors
- I2C environmental sensors

All extensions use the generic data-source/channel/capability model rather than application-specific hardcoding.
