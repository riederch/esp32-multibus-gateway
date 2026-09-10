# Validation tasks

Open technical items that require measurement, interoperability testing or post-layout validation.

## VE.Bus electrical bring-up

For the Victron MultiPlus 12/500/20-16 validate on the assembled Rev-A carrier:

- actual RJ45 pinout on the target device
- A/B polarity
- idle/common-mode signal levels
- native bias/termination behaviour
- receive/transmit turnaround timing
- explicit DE/RE timing margin at 256000 baud
- operation in on/off/standby/low-power states

Begin with passive receive/sniffing before enabling transmission.

## VE.Bus optional power and control

Measure before populating optional Rev-A footprints:

- RJ45 V+ voltage over all MultiPlus operating states
- available current and source impedance
- STB electrical levels and required drive behaviour
- PD electrical levels and required sense/drive behaviour

The Rev-A PCB already reserves protected power and isolated STB/PD interface options; these measurements determine population, not PCB topology.

## Power / thermal validation

On the assembled carrier validate:

- correct operation from 5 V through 30 V input
- TPS2660 current limiting and reverse-polarity behaviour
- LMR38020 4.75-V regulation and startup
- USB/carrier simultaneous connection with no back-feed
- BAT/SOL coexistence with the selected Heltec V4.2 board
- regulator and eFuse temperatures at worst-case load and ambient
- conducted/radiated switching noise near LoRa and GNSS

## Modbus RS485 bring-up

Validate the fixed Rev-A ISOW1412 implementation:

- A/B polarity
- DE/RE timing
- 9.6/19.2/115.2 kbit/s operation and margin testing up to 256 kbit/s
- selectable 120-ohm termination
- optional 680-ohm fail-safe bias
- operation with bias provided by another node
- ESD/EFT behaviour at the field connector
- communication with representative deployed meters/drives

## GNSS module

Validate the exact external module selected for deployment:

- supply requirements
- default baud/protocol
- supported constellations
- PPS behaviour
- wake/reset/power-control behaviour
- current consumption

Carrier PCB routing is not dependent on this choice.

## Native USB / MK3 compatibility

Validate:

- MK3 USB descriptors/interfaces
- host-driver expectations
- FTDI-specific control behaviour if required
- MK2/MK3 byte-stream transport
- VictronConnect interoperability

A genuine FTDI USB-UART implementation remains an acceptable future hardware variant if host compatibility requires it.

## Victron BLE compatibility

Validate:

- advertising/services
- pairing/security behaviour
- values and commands expected by VictronConnect
- achievable Smart-Dongle compatibility scope

## LoRaWAN stack

Validate:

- EU868
- OTAA
- Class A / C
- confirmed/unconfirmed uplinks
- ADR
- persistent frame counters
- join/rejoin behaviour
- ChirpStack interoperability
- payload size/fragmentation
- FUOTA feasibility
- FPort-85 compatibility vectors against the complete applicable UC100 V2 command set

## Storage

Validate:

- history ring-buffer layout
- wear levelling
- atomic updates
- power-loss behaviour during writes/restores
- flash endurance at worst-case write rates

## Web security

Validate/harden:

- password KDF/work factor
- session lifecycle
- CSRF protection
- login throttling
- backup encryption
- HTTPS/certificate strategy

## EMC / environmental pre-compliance

Before calling Rev A production-ready validate:

- ESD at field and RJ45 connectors
- EFT/burst on field supply and RS485
- surge strategy appropriate to the deployment environment
- conducted emissions of the buck converter and both isolated transceivers
- radiated emissions / immunity interaction with LoRa, Wi-Fi and GNSS
- thermal soak and cold-start behaviour

## Completion rule

A resolved item moves into the relevant permanent specification or hardware document and is removed from this list. Hardware topology already frozen in `docs/hardware-rev-a.md` is not reopened by ordinary bring-up measurements unless the validation result demonstrates a design defect.