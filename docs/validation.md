# Validation tasks

Open technical items that require measurement, interoperability testing or post-layout validation.

## VE.Bus electrical bring-up

For the target Victron MultiPlus validate on the assembled Rev-A carrier:

- actual RJ45 pinout on the target device
- A/B polarity
- idle/common-mode signal levels
- native bias/termination behaviour
- receive/transmit turnaround timing
- explicit DE/RE timing margin at 256000 baud
- operation in on/off/standby/low-power states

Begin with passive receive/sniffing before enabling transmission.

## VE.Bus power and control

Measure before selecting/populating the VE.Bus power module and STB/PD interfaces:

- RJ45 `VEBUS_VPLUS` voltage in all MultiPlus operating states
- available current and source impedance
- isolated DC/DC input range required by the measured V+
- isolated-converter efficiency and startup behavior at minimum source voltage
- STB electrical levels and required drive behaviour
- PD electrical levels and required sense/drive behaviour

The Rev-A topology already reserves an isolated VE.Bus power path and isolated STB/PD options; measurements select the components/population.

## Power / thermal validation

On the assembled carrier validate:

- operation over the specified external 5-30 V input range
- TPS26600 current limiting and reverse-polarity behaviour
- correct separation of `U1_RTN` from `CORE_GND`
- LMR38020 regulation/startup at 400 kHz
- `EXT_5V` set point and ripple
- `D_EXT` forward drop, reverse leakage and thermal rise
- isolated VE.Bus DC/DC regulation, isolation and startup after U7 is selected
- `D_VE` forward drop, reverse leakage and thermal rise
- stable `SYS_5V` with external and VE.Bus sources individually and simultaneously
- `JP_USB_SAFE` closed for field power and open before USB service
- with `JP_USB_SAFE` open, no carrier-source voltage present at `HELTEC_5V`
- with USB connected and `JP_USB_SAFE` open, no reverse current into either carrier power branch
- BAT/SOL coexistence with the selected Heltec V4.2 board
- regulator, OR-diode and eFuse temperatures at worst-case load/ambient
- conducted/radiated switching noise near LoRa and GNSS

Do not validate USB by connecting USB while `JP_USB_SAFE` is closed; the Heltec V4.2 documentation does not permit simultaneous USB and external 5-V-pin powering.

## Modbus RS485 bring-up

Validate the fixed Rev-A ISOW1412 implementation:

- VIO at 3.3 V and VDD at `SYS_5V`
- isolated 5-V supply generated correctly with MODE tied for 5-V operation
- TI-recommended VDD/VISO decoupling and optional ferrite paths populated as intended
- A/B polarity
- DE/RE timing and safe reset state
- 9.6/19.2/115.2 kbit/s operation and margin testing up to 256 kbit/s
- selectable 120-ohm termination
- optional 680-ohm fail-safe bias
- operation with bias supplied by another node
- SM712 protection orientation/footprint
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

## KiCad / PCB release

Before layout/release:

- import/convert `hardware/rev-a/multibus-rev-a-detail.sch` in the target KiCad version
- verify all custom symbol pin numbers against the exact manufacturer package suffixes
- assign footprints
- verify connector physical pin numbering, especially VE.Bus RJ45
- run ERC with zero unexplained errors
- preserve isolation-barrier clearances in schematic/PCB net classes
- check differential-pair and connector placement rules
- generate and review BOM/netlist before routing

## EMC / environmental pre-compliance

Before calling Rev A production-ready validate:

- ESD at field and RJ45 connectors
- EFT/burst on field supply and RS485
- surge strategy appropriate to the deployment environment
- conducted emissions of the external buck, isolated VE.Bus power converter and both isolated transceivers
- radiated emissions / immunity interaction with LoRa, Wi-Fi and GNSS
- thermal soak and cold-start behaviour

## Completion rule

A resolved item moves into the relevant permanent specification or hardware document and is removed from this list. Hardware topology frozen in `docs/hardware-rev-a.md` is not reopened by ordinary bring-up measurements unless validation demonstrates a design defect.
