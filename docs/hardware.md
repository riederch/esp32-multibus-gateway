# Hardware notes

## Prototype

Recommended first prototype:

- ESP32-S3 DevKitC-1 or compatible development board
- two separate galvanically isolated TTL-to-RS485 modules
- one RJ45 breakout for VE.Bus
- one terminal block for generic RS485 / Modbus
- one 9-30 V -> 5 V DC/DC buck converter, preferably rated for at least 1 A and ideally 2 A

## VE.Bus channel

Requirements:

- isolated RS485 transceiver path
- 256000 baud capability
- explicit TX driver-enable control preferred
- termination/biasing must match the actual VE.Bus topology

A generic automatic-direction RS485 module may be useful for initial experiments, but should not be assumed suitable for final VE.Bus hardware until turnaround timing has been measured.

### VE.Bus RJ45 pinout

| RJ45 pin | Signal | T568B wire colour | Prototype use |
|---:|---|---|---|
| 1 | NC | white/orange | leave open |
| 2 | V+ | orange | optional gateway supply source |
| 3 | GND | white/green | VE.Bus reference / supply return |
| 4 | A | blue | VE.Bus differential data A |
| 5 | B | white/blue | VE.Bus differential data B |
| 6 | STB / Standby | green | leave open initially |
| 7 | PD / Panel Detect | white/brown | leave open initially |
| 8 | NC | brown | leave open |

For the first communication tests, only pins 4 (A), 5 (B) and, where required by the transceiver topology, pin 3 (GND) are needed on the bus side.

Pins 6 (Standby) and 7 (Panel Detect) remain unconnected until the corresponding control behaviour is explicitly implemented and tested.

Do not confuse VE.Bus with VE.Can. They use the same RJ45 connector family but different electrical pin assignments and protocols.

## Gateway power supply

The gateway shall not be designed around a fixed regulated 12 V input. Instead, use a **wide-input DC/DC buck converter with approximately 9-30 V input and regulated 5 V output**.

This allows the same hardware to be supplied from either:

1. the local 12 V battery/system supply, or
2. VE.Bus V+ on RJ45 pin 2 with GND on pin 3, if the target Victron device provides sufficient load capability.

The 5 V converter should be rated for at least **1 A**, with **2 A preferred** to provide margin for ESP32-S3 Wi-Fi/BLE current peaks and both interface modules.

### VE.Bus supply note

VE.Bus exposes V+ on pin 2 and GND on pin 3. On a 12 V MultiPlus this should be treated as a battery-derived, non-regulated supply rather than as a guaranteed fixed 12 V auxiliary output. Expected voltage is therefore in the general battery-system range, not exactly 12.0 V.

Before VE.Bus pin 2 is used as the normal gateway supply, measure and validate on the target MultiPlus:

- open-circuit voltage pin 2 to pin 3
- voltage under approximately 100 mA load
- voltage under approximately 250 mA load
- optionally voltage under approximately 500 mA load

Until this is verified, the preferred prototype supply is the battery through the same 9-30 V -> 5 V converter.

### Final PCB input protection

For a later PCB, provide:

- 9-30 V DC input capability
- input fuse
- reverse-polarity protection
- transient/TVS protection
- regulated 5 V rail
- 3.3 V regulation as required by the ESP32-S3 implementation
- USB-C for development and diagnostics

## Modbus channel

Requirements:

- separate isolated RS485 transceiver
- configurable baud rate, parity and stop bits
- A/B/GND terminal connection
- optional 120 ohm termination selectable by jumper or switch

The Modbus and VE.Bus interfaces must remain electrically and logically independent and must not share one switched RS485 transceiver.

## Isolation

The intended final PCB should provide separate galvanic isolation for:

1. VE.Bus <-> ESP32 logic
2. Modbus RS485 <-> ESP32 logic

This prevents ground loops between inverter/charger equipment and external field devices.

## Target device

Initial VE.Bus target:

- Victron MultiPlus 12/500/20-16

No assumption should be made that all VE.Bus device generations behave identically.
