# Hardware notes

## Prototype

Recommended first prototype:

- ESP32-S3 DevKitC-1 or compatible development board
- two separate galvanically isolated TTL-to-RS485 modules
- one RJ45 breakout for VE.Bus
- one terminal block for generic RS485 / Modbus

## VE.Bus channel

Requirements:

- isolated RS485 transceiver path
- 256000 baud capability
- explicit TX driver-enable control preferred
- RJ45 wiring must be verified before connection
- termination/biasing must match the actual VE.Bus topology

A generic automatic-direction RS485 module may be useful for initial experiments, but should not be assumed suitable for final VE.Bus hardware until turnaround timing has been measured.

## Modbus channel

Requirements:

- separate isolated RS485 transceiver
- configurable baud rate, parity and stop bits
- A/B/GND terminal connection
- optional 120 ohm termination selectable by jumper or switch

## Isolation

The intended final PCB should provide separate galvanic isolation for:

1. VE.Bus <-> ESP32 logic
2. Modbus RS485 <-> ESP32 logic

This prevents ground loops between inverter/charger equipment and external field devices.

## Power

For a later DIN-rail board, consider:

- 9-30 V DC input
- protected buck converter to 5 V / 3.3 V
- USB-C for development and diagnostics
- optional isolated DC/DC domains for each bus channel

## Target device

Initial VE.Bus target:

- Victron MultiPlus 12/500/20-16

No assumption should be made that all VE.Bus device generations behave identically.
