# Validation tasks

Open technical items that require measurement, interoperability testing or final hardware selection.

## VE.Bus electrical interface

For the Victron MultiPlus 12/500/20-16 validate:

- RJ45 pinout on the actual device
- V+ voltage and available current
- behaviour in on/off/standby/low-power states
- A/B polarity
- signal levels
- bias/termination behaviour
- receive/transmit turnaround timing
- explicit DE/RE timing

Begin with passive receive/sniffing before enabling transmission.

## Power architecture

Validate a final topology for:

- external 5-30 V input
- optional VE.Bus-derived supply
- USB-C
- BAT / SOL board paths

Requirements:

- no back-feed between sources
- reverse-polarity protection
- transient protection
- safe source selection / ORing
- galvanic isolation compatible with USB
- acceptable efficiency and quiescent current

## Modbus RS485 hardware

Validate:

- isolated transceiver selection
- 3.3 V logic compatibility
- DE/RE timing
- selectable 120-ohm termination
- fail-safe biasing
- A/B polarity
- maximum baud rate
- final free GPIO allocation

## GNSS module

Validate the exact external module:

- supply requirements
- default baud/protocol
- supported constellations
- PPS behaviour
- wake/reset/power-control behaviour
- current consumption

## Native USB / MK3 compatibility

Validate:

- MK3 USB descriptors/interfaces
- host-driver expectations
- FTDI-specific control behaviour if required
- MK2/MK3 byte-stream transport
- VictronConnect interoperability

A genuine FTDI USB-UART implementation remains an acceptable hardware fallback if required for host compatibility.

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
- ABP if required
- Class A / C
- confirmed/unconfirmed uplinks
- ADR
- persistent frame counters
- join/rejoin behaviour
- ChirpStack interoperability
- payload size/fragmentation
- FUOTA feasibility
- FPort-85 compatibility vectors against the complete UC100 V2 command set

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

## Completion rule

A resolved item moves into the relevant permanent specification or hardware document and is removed from this list.
