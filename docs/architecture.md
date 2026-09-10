# Architecture

## Product concept

The installed gateway should be a compact two-field-connection device:

```text
                       Bluetooth LE
                            )))
                             |
                    +----------------+
VE.Bus RJ45 --------|                |-------- USB-C (service only)
(data + power)      |    ESP32-S3    |
                    |                |
Modbus A/B ---------|                |
                    +----------------+
```

Normal operation requires only:

1. **one VE.Bus RJ45 cable** to the Victron device, carrying VE.Bus communication and gateway power
2. **one Modbus RS485 terminal connection** to the external field bus

Bluetooth LE and USB-C are service/configuration interfaces and require no additional permanent field wiring.

## Electrical domains

The gateway uses two independent RS485 paths:

- UART1 -> isolated RS485 -> VE.Bus (RJ45)
- UART2 -> isolated RS485 -> Modbus RTU (A/B, optional isolated reference)

The buses must not share one switched transceiver.

The ESP32-S3/system/USB side should be galvanically isolated from the VE.Bus electrical domain. VE.Bus V+ and GND therefore power the system through an isolated DC/DC converter, while VE.Bus A/B cross the same isolation boundary through an isolated RS485 transceiver. The Modbus port receives its own independent isolation.

## Functional blocks

```text
VE.Bus RJ45
  |- V+ / GND -> protection -> isolated DC/DC -> 5 V system rail
  |- A / B    -> isolated VE.Bus RS485 transceiver -> UART1
  |- STB / PD -> optional later isolated control implementation

ESP32-S3
  |- VE.Bus service
  |    |- transport/timing
  |    |- frame parsing
  |    |- variable/settings access
  |
  |- Modbus service
  |    |- UART2
  |    |- isolated RS485
  |    |- RTU master
  |    |- optional RTU slave
  |
  |- Bluetooth LE
  |    |- commissioning/diagnostics
  |    |- later VictronConnect Smart-Dongle compatibility experiment
  |
  |- Native USB-C device
  |    |- diagnostics / firmware service
  |    |- MK2/MK3 protocol transport
  |    |- experimental MK3-USB device emulation
  |
  |- Network services (optional)
       |- Web UI
       |- MQTT
```

## USB / MK3 compatibility strategy

The original Victron MK3-USB contains an FTDI USB-to-serial interface in front of the MK2/MK3 protocol implementation. The ESP32-S3 has a native USB-OTG device peripheral and can implement custom USB descriptors, CDC and vendor-specific USB classes.

Therefore the project should implement USB compatibility in layers:

```text
VictronConnect
      |
 USB device transport
      |
+-------------------------+
| MK3 USB compatibility   |
| - USB descriptors       |
| - FTDI-like control     |
|   requests if required  |
| - byte stream transport |
+------------+------------+
             |
+------------v------------+
| MK2/MK3 protocol engine |
+------------+------------+
             |
+------------v------------+
| direct VE.Bus engine    |
+------------+------------+
             |
          MultiPlus
```

### Native ESP32-S3 approach

Preferred research path: use the ESP32-S3 native USB port and emulate enough of the MK3 USB-facing behaviour for VictronConnect to accept it.

This is technically feasible at the USB-device level because the ESP32-S3 USB stack supports custom VID/PID/descriptors and vendor-specific device functions. However, a normal USB CDC-ACM serial device is **not automatically equivalent to an FTDI FT232 device**. If VictronConnect or the host FTDI driver relies on FTDI-specific USB control requests, those requests must also be emulated.

USB VID/PID values belonging to another vendor must not be used in a distributable product without authorization. Descriptor spoofing may be useful only as a controlled interoperability experiment.

### Compatibility fallback

If native USB emulation proves unreliable, a later PCB can add a genuine FTDI USB-UART device between USB-C and the ESP32. The ESP32 would still implement all MK2/MK3 protocol logic; the FTDI chip would only provide the host-facing USB serial behaviour. This is the lowest-risk route to close compatibility with software expecting an MK3-style FTDI serial interface.

Keep the MK2/MK3 protocol engine independent of either USB backend so both strategies can be tested without redesigning the VE.Bus layer.

## Development phases

### Phase 1 - Electrical bring-up

- validate VE.Bus pin 2/3 voltage and available current on MultiPlus 12/500/20-16
- validate isolated VE.Bus-powered DC/DC supply
- verify ESP32-S3 UART operation at 256000 baud
- verify isolated RS485 interface timing
- passive VE.Bus sniffing only

### Phase 2 - VE.Bus receive path

- identify valid frames
- decode basic measurements
- expose diagnostics over USB serial / BLE

### Phase 3 - VE.Bus write path

- implement controlled transmit/turnaround
- test non-destructive commands first
- add settings read/write
- initial target: charger current on MultiPlus 12/500/20-16

### Phase 4 - Modbus RTU

- add independent configurable RTU master
- baud/parity/stopbits configurable
- bus scan and generic register access

### Phase 5 - Bluetooth and network services

- BLE commissioning/diagnostics
- Web UI and MQTT if required
- configuration persistence

### Phase 6 - Victron compatibility experiments

- implement MK2/MK3 protocol engine independent of transport
- expose it over native ESP32-S3 USB
- test recognition by VictronConnect
- implement FTDI-compatible vendor requests if required
- use genuine FTDI hardware as fallback if host compatibility requires it
- optional VictronConnect BLE / Smart-Dongle emulation after core VE.Bus functionality is stable

## Safety boundaries

- Keep VE.Bus galvanically isolated from ESP32/system/USB and from the Modbus side.
- Do not depend on automatic TX/RX direction for final VE.Bus hardware unless timing is proven.
- Use explicit DE/RE control for the production VE.Bus transceiver.
- Treat direct VE.Bus control and Victron protocol emulation as experimental until validated on controlled hardware.
