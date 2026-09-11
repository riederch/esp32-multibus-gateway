# Rev-A PCB footprint and mechanical baseline

This file defines the first PCB-placement baseline for the MultiBus Gateway Rev A carrier. It is intentionally conservative: only dimensions and footprints that have been verified against current manufacturer data are frozen here.

## Heltec WiFi LoRa 32 V4.2 carrier

Reference module: Heltec HTIT-WB32LAF V4.2 / WiFi LoRa 32 V4.2.

Verified physical baseline:

```text
board outline       51.69 mm x 25.40 mm
header pitch        2.54 mm
rows                J2 + J3, 18 pins each
row spacing         22.86 mm
pin span per row    43.18 mm
```

The local footprint is `Heltec_WiFi_LoRa_32_V4_2` in `multibus-rev-a.pretty`.

Orientation convention used by the footprint:

- USB-C is at negative Y / the lower end of the footprint
- J3 is the left header row
- J2 is the right header row
- physical pin 1 of each row is at the USB-C end

The footprint uses 1.0-mm drills and 1.8-mm through-hole pads for standard 2.54-mm pin/socket headers. Before production release, confirm the actual mating socket/header body dimensions selected for the carrier.

The schematic symbol `HELTEC_V4_2_PHYSICAL` exposes all 36 physical header pins. The previous compact carrier symbol remains useful for logical diagrams but must not be the only symbol used for PCB netlisting.

## Frozen connector choices

### J1 field connector

Preferred production part:

```text
Phoenix Contact 1757268
MSTBA 2,5/4-G-5,08
4 positions, 5.08-mm pitch
```

Use the manufacturer mechanical drawing or Phoenix-provided ECAD model for the final footprint. Keep the connector at a board edge with wire entry unobstructed.

### J2 VE.Bus connector

Preferred production part:

```text
Wuerth Elektronik 615008143721
WR-MJ, 8P8C, unshielded, right-angle THT
```

Use the manufacturer-provided footprint/3D model where possible. The unshielded version avoids creating an undefined chassis/shield reference in Rev A.

## IC package assignments

```text
U1 TPS26600PWP     TI PWP / HTSSOP-16 PowerPAD
U2 LMR38020SDDAR   TI DDA / 8-pin PowerPAD SOIC
U3 ISOW1412DFM     TI DFM / 20-pin wide SOIC
U5 ISOW1412DFM     TI DFM / 20-pin wide SOIC
U4/U6 SM712        SOT-23
D_EXT/D_VE         PMEG3050EP / SOD128 candidate
```

For U1/U2/U3/U5, prefer the exact TI recommended land pattern over a merely package-name-compatible generic footprint.

## Board architecture

Preferred stackup: 4 layers.

Suggested functional placement from field side toward RF side:

```text
J1 field connector
  |
input protection / TPS26600
  |
LMR38020 power stage
  |
Modbus ISOW1412 + TVS/termination
  |
Core / Heltec socket region
  |
VE.Bus ISOW1412 + isolated VE.Bus power
  |
J2 RJ45
```

Do not force this linear arrangement if it worsens isolation or RF clearance; isolation barriers and antenna keep-out take precedence.

## Isolation rules

Maintain separate electrical domains:

```text
CORE_GND
MB_GISO
VE_GISO
VEBUS_GND
```

Rules:

- no copper pour, trace, stitching via or test-pad copper may cross an ISOW1412 isolation barrier
- no direct copper joins `VEBUS_GND` to `CORE_GND`
- the VE.Bus power converter must preserve the same field-to-Core isolation concept as the VE.Bus data interface
- keep the Modbus isolated-side copper physically distinct from the VE.Bus isolated-side copper
- preserve the manufacturer-recommended creepage/clearance around U3/U5 and U7; do not reduce it to ordinary signal clearance

## Power-layout rules

LMR38020:

- VIN bypass capacitors immediately adjacent to VIN/GND
- bootstrap capacitor adjacent to BOOT/SW
- minimize the SW-node copper area
- place L1 immediately at SW
- place COUT directly after L1 with short return to power ground
- keep feedback divider away from the SW node and sense the quiet output node

TPS26600:

- place TVS/fuse at the connector before long PCB traces
- keep high-current IN/OUT paths short and wide
- keep `U1_RTN` physically and electrically distinct from `CORE_GND` as required by the reverse-polarity topology
- provide adequate copper for the exposed pad according to TI guidance

## RS485-layout rules

For both buses:

- place SM712 close to the external connector side of the interface
- keep A/B routing together and symmetric
- keep stubs to termination, bias and test points short
- place termination directly across A/B
- place bias resistors on the isolated side only

Modbus protection is physically closest to J1. VE.Bus protection is physically closest to J2.

## RF/mechanical keep-outs

- preserve the complete Heltec LoRa antenna keep-out and avoid copper/ground/tall components in that region
- do not put LMR38020, L1, isolated DC/DC magnetics or RS485 transformers/inductors under either RF antenna region
- preserve direct access to Heltec USB-C
- preserve access to USER/PRG controls and OLED
- place `JP_USB_SAFE` where it can be reached without removing the Heltec module
- silkscreen `OPEN FOR USB` next to `JP_USB_SAFE`

## Test-point placement

Required test points must be reachable with the Heltec fitted:

```text
VIN_FIELD
VPROTECTED
EXT_5V
SYS_5V
CORE_GND
3V3
MB_A
MB_B
MB_VISO
MB_GISO
MB_TX
MB_RX
MB_DIR
VE_A
VE_B
VE_VISO
VE_GISO
VE_TX
VE_RX
VE_DIR
VEBUS_VPLUS
VEBUS_GND
VE_ISO_5V
```

## Still blocking final PCB release

The PCB must not be called production-ready until all of these are resolved:

1. open/convert the detailed schematic in a current KiCad release
2. replace the logical Heltec symbol in the netlisted design with the 36-pin physical symbol and verify every used pin
3. run ERC with no unexplained errors
4. assign and verify all manufacturer land patterns
5. measure `VEBUS_VPLUS` and select U7 plus its protection
6. validate the selected Heltec mating sockets mechanically
7. validate antenna keep-out against the final enclosure
8. run DRC after routing and inspect both isolation barriers manually
9. generate Gerber/drill/BOM/position outputs and review them before ordering
