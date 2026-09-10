# Architecture

## Core principle

The gateway uses two independent UART + RS485 paths:

- UART1 -> isolated RS485 -> VE.Bus (RJ45)
- UART2 -> isolated RS485 -> Modbus RTU (A/B/GND)

The buses must not share one switched transceiver.

## Functional blocks

```text
ESP32-S3
  |- VE.Bus service
  |    |- transport/timing
  |    |- frame parsing
  |    |- variable/settings access
  |
  |- Modbus service
  |    |- RTU master
  |    |- optional RTU slave
  |
  |- Network services
       |- Web UI
       |- MQTT
       |- optional BLE
       |- optional USB MK2/MK3 compatibility layer
```

## Development phases

### Phase 1 - Electrical bring-up

- verify ESP32-S3 UART operation at 256000 baud
- verify isolated RS485 interface timing
- verify RJ45 pinout against Victron documentation and reference hardware
- passive sniffing only

### Phase 2 - VE.Bus receive path

- identify valid frames
- decode basic measurements
- expose diagnostics over USB serial

### Phase 3 - VE.Bus write path

- implement controlled transmit/turnaround
- test non-destructive commands first
- add settings read/write
- initial target: charger current on MultiPlus 12/500/20-16

### Phase 4 - Modbus RTU

- add independent configurable RTU master
- baud/parity/stopbits configurable
- bus scan and generic register access

### Phase 5 - Network services

- Web UI
- MQTT publish/subscribe
- configuration persistence

### Phase 6 - Compatibility experiments

- USB MK2/MK3-compatible transport
- optional VictronConnect BLE/Smart-Dongle emulation

## Safety boundaries

- Keep VE.Bus galvanically isolated from ESP32 logic and from the Modbus side.
- Do not depend on automatic TX/RX direction for final VE.Bus hardware unless timing is proven.
- Use explicit DE/RE control for the production VE.Bus transceiver.
- Treat direct VE.Bus control as experimental until tested against a sacrificial or controlled setup.
