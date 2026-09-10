# Validation tasks

These items are intentionally left as implementation validation tasks. They do not block the V1 architecture/specification.

## HTIT-WB32LAF V4.2 resource mapping

Validate the exact board revision and create a final GPIO/resource map for:

- SX1262
- OLED
- native USB D+/D-
- user button
- LEDs
- battery measurement control
- Vext control
- GNSS UART/PPS/power-control signals
- VE.Bus UART RX/TX/DE/RE
- Modbus UART RX/TX/DE/RE
- remaining free GPIO/ADC/I2C/SPI/UART resources

No production pin assignment is considered final until this is complete.

## Ordered GNSS module

Identify the exact external GNSS module supplied/ordered with the board and validate:

- supply voltage
- UART voltage levels
- baud rate/default protocol
- supported constellations
- PPS availability
- wake/reset/power-control behaviour
- current consumption
- connector pinout

## VE.Bus electrical interface

For the Victron MultiPlus 12/500/20-16 validate:

- RJ45 pinout on actual hardware before connection
- V+ voltage on pin 2 relative to pin 3
- available current/power from VE.Bus V+
- behaviour in on/off/standby/low-power states
- A/B polarity
- signal levels
- required bias/termination behaviour
- receive/transmit turnaround timing
- suitability of prototype automatic-direction RS485 modules
- explicit DE/RE timing for final hardware

Start with passive receive/sniffing before enabling transmission.

## Power architecture

Validate a final topology supporting both:

- external 5-30 V field-terminal input
- optional VE.Bus-derived supply

Requirements:

- no source back-feed
- reverse-polarity protection
- input transient protection
- safe power ORing/source selection
- galvanic isolation strategy compatible with USB connection
- acceptable efficiency and quiescent current
- safe interaction with HTIT-WB32LAF BAT/SOL/USB power paths

## Modbus RS485 hardware

Validate:

- selected isolated transceiver module/device
- 3.3 V ESP32 logic compatibility
- 5-30 V field-system isolation behaviour
- DE/RE timing
- selectable 120 ohm termination
- fail-safe biasing strategy
- A/B naming/polarity
- maximum supported baud rate

## Native USB / MK3 compatibility

Research and validate whether Victron software accepts a native ESP32-S3 USB implementation.

Validate:

- original MK3 USB descriptors/interfaces
- host-driver expectations
- whether a normal CDC interface is sufficient
- required FTDI-like vendor control requests
- MK2/MK3 byte-stream protocol transport
- behaviour with VictronConnect on supported desktop/mobile hosts

Do not use another vendor's VID/PID in a distributable product without authorization.

Fallback: genuine FTDI USB-UART hardware on a future PCB while keeping the ESP32-side MK2/MK3 protocol engine unchanged.

## Victron BLE compatibility

Reverse-engineering/interop research task:

- identify Smart Dongle advertising/service behaviour
- determine pairing/security requirements
- determine which values/commands VictronConnect expects
- establish realistic compatibility scope

This is part of the Victron cocoon but is not required to validate basic VE.Bus communication.

## LoRaWAN stack

Validate selected ESP32-S3/SX1262 LoRaWAN stack for:

- EU868
- OTAA
- ABP if required
- Class A
- Class C
- downlink handling
- confirmed/unconfirmed uplinks
- ADR
- persistent frame counters
- join/rejoin behaviour
- ChirpStack interoperability
- payload size/fragmentation strategy
- FUOTA feasibility

## Storage endurance

Define and test:

- NVS/config storage strategy
- history ring-buffer layout
- wear levelling
- atomic configuration updates
- power-loss behaviour during writes/restores
- expected flash endurance under worst-case polling/history rates

## Web security implementation

Validate practical embedded implementation for:

- password hashing/work factor
- session storage
- CSRF protection
- login rate limiting/lockout
- backup encryption format
- HTTPS feasibility/certificate strategy if enabled

## Acceptance criterion

Each validation item should eventually resolve to one of:

- confirmed and documented
- changed design decision
- explicitly unsupported limitation

Resolved items should be moved into the relevant permanent specification/hardware document rather than remaining only in this file.
