# Hardware Rev A

This document is the schematic-capture specification for the first MultiBus carrier PCB around the Heltec HTIT-WB32LAF V4.2.

## 1. Functional blocks

```text
J1 FIELD INPUT
V+ / V- / A / B
   |
   +-- protected power --> external DC/DC --> D_EXT --+
   |                                                   +--> SYS_5V --> Heltec 5V
   `-- isolated Modbus A/B --> ESP32-S3 UART           |
                                                       |
J2 VE.BUS RJ45                                        |
   |                                                   |
   +-- isolated VE.Bus A/B --> ESP32-S3 UART          |
   +-- STB/PD optional isolated footprints            |
   `-- V+ / GND --> isolated DC/DC --> D_VE ----------+
```

The raw external and VE.Bus supply domains are never tied together. VE.Bus power crosses a galvanically isolated DC/DC converter before reaching the Core-side `SYS_5V` rail.

## 2. Mandatory schematic sheets

Use these logical sheets when capturing the design:

1. `01_heltec_carrier`
2. `02_field_power`
3. `03_modbus_rs485`
4. `04_vebus_rs485`
5. `05_connectors_testpoints`

## 3. Field connector J1

Four-pole 5.08-mm pluggable terminal block.

```text
J1.1  V+
J1.2  V-
J1.3  MODBUS_A
J1.4  MODBUS_B
```

Preferred family: Phoenix Contact MSTBA 2,5/4-G-5,08 or footprint-compatible equivalent.

## 4. Field power

### 4.1 Input protection

Net sequence:

```text
V+ -> fuse/PTC option -> TVS -> TPS2660 -> VPROTECTED
V- ----------------------------------------> CORE_GND
```

Use TPS2660 as the main electronic protection element.

Manufacturer verification notes for schematic capture:

- TPS2660 operating range is 4.2-60 V.
- HTSSOP/PWP package has duplicated `IN` pins 1/2 and duplicated `OUT` pins 15/16.
- `UVLO`, `OVP`, `ILIM`, `dVdT`, `MODE`, `SHDN`, `RTN`, `GND`, `IMON` and `FLT` must be represented explicitly in the symbol.
- PowerPad belongs to the `RTN` plane and must not be the sole RTN connection.

Default design intent:

```text
current limit = approximately 1 A
UVLO          = external divider/configuration allowing operation from 5 V
OVP           = threshold above 30 V and below downstream absolute maximum
```

The exact `RILIM`, UVLO/OVP divider and dVdT components are schematic-calculation items and must be checked against the chosen TPS2660 suffix before PCB release. Do not encode the earlier provisional `11.8 kOhm` value as an unchecked production value.

Do not use the device's internal 15-V UVLO mode.

Provide footprints for:

- resettable fuse or replaceable fuse at J1
- bidirectional input TVS sized for the selected OVP strategy
- 100 nF / 50 V ceramic
- 4.7 uF / 50 V ceramic
- 47-100 uF bulk capacitor after protection
- `VIN_FIELD` and `VPROTECTED` test points

### 4.2 External buck converter

U2: TI LMR38020.

Manufacturer-verified package/pin assignment:

```text
1  GND
2  EN
3  VIN
4  RT/SYNC
5  FB
6  PG
7  BOOT
8  SW
EP GND
```

Design target:

```text
VIN   = VPROTECTED
VOUT  = EXT_5V, selected with D_EXT drop so SYS_5V remains valid for the Heltec
fSW   = 400 kHz
IOUT  = design for up to 2 A capability
```

The TI 5-V / 400-kHz reference point uses:

```text
RFBT  = 100 kOhm, 1 %
RFBB  = 24.9 kOhm, 1 %
L1    = 15 uH
COUT  = 3 x 22 uF nominal
CBOOT = 100 nF
```

The final feedback divider may be adjusted from the 5.00-V reference if required to compensate the selected `D_EXT` forward drop. Do not retain the previous hard-coded 4.75-V pre-OR assumption.

The switching-frequency programming resistor on `RT/SYNC` must be selected from the current TI design equation/table during schematic capture; the earlier provisional `64.9 kOhm` value is not frozen until checked against the exact selected variant and desired mode.

Follow the LMR38020 layout example for the VIN-SW-inductor-output current loop.

### 4.3 Low-voltage source ORing

The external and VE.Bus branches are ORed only after conversion to Core-side low voltage:

```text
EXT_5V    -> D_EXT --+
                     +--> SYS_5V --> Heltec 5V
VE_ISO_5V -> D_VE ---+
```

`D_EXT` and `D_VE` are separate one-way source-OR elements. The first Rev-A implementation may use appropriately rated low-loss Schottky diodes. Their forward drop, current rating, leakage and thermal rise must be verified with the final Heltec load.

The previously considered LM66100/LM66200 stage is not part of the Rev-A power architecture.

### 4.4 USB coexistence

USB-C remains native on the Heltec board. The carrier connects only through `SYS_5V` and its one-way source ORing.

Required behaviour:

- USB must not back-feed `EXT_5V` or the isolated VE.Bus converter output.
- Neither carrier source may back-feed the other source.
- Set the carrier-side voltage hierarchy so USB normally wins when attached for service/programming.

## 5. Modbus isolated RS485

U3: ISOW1412.

Manufacturer-verified 20-pin DFM pin assignment:

```text
1  VIO
2  D
3  DE
4  R
5  /RE
6  GNDIO
7  OUT
8  EN/FLT
9  VDD
10 GND1
11 GND2
12 VISOOUT
13 MODE
14 IN
15 GISOIN
16 VISOIN
17 Y
18 Z
19 B
20 A
```

`GNDIO` and `GND1` are shorted on the Core side. `GND2` and `GISOIN`, and `VISOOUT` and `VISOIN`, are connected as required by the TI application circuit, directly or through the recommended ferrite option.

### 5.1 Logic side

```text
GPIO4  -> MODBUS_TX -> D
GPIO2  <- MODBUS_RX <- R
GPIO5  -> MODBUS_DIR -> DE and /RE control
3V3    -> VIO / logic supply
CORE_GND -> GNDIO / GND1
```

Implement deterministic TX/RX direction control. At reset, driver must default disabled and receiver enabled. The ISOW1412 itself has an internal pull-down on `DE` and pull-up on `/RE`; external logic must not defeat the safe reset state.

### 5.2 Isolated bus side

Configure half duplex as:

```text
Y + A -> MODBUS_A
Z + B -> MODBUS_B
```

The RS485 side remains electrically isolated from `CORE_GND` and J1 `V-`.

### 5.3 Protection / termination

Place directly at J1:

- U4: SM712 or equivalent dedicated RS485 TVS
- RTERM: 120 Ohm, enabled by solder jumper `SJ_MB_TERM`
- RBIAS_PU: 680 Ohm from `MB_VISO` to A through `SJ_MB_BIAS_PU`
- RBIAS_PD: 680 Ohm from B to `MB_GISO` through `SJ_MB_BIAS_PD`

Default assembly for a gateway/master prototype:

```text
termination  = populated resistor, jumper open
bias resistors = populated, jumpers open
```

This makes the PCB flexible; enable termination/bias only when required by installation topology.

## 6. VE.Bus isolated interface

J2: 8P8C RJ45, unshielded preferred unless the enclosure provides a defined chassis/shield concept.

```text
1 NC
2 VEBUS_VPLUS
3 VEBUS_GND
4 VEBUS_A
5 VEBUS_B
6 VEBUS_STB
7 VEBUS_PD
8 NC
```

U5: second ISOW1412, electrically independent from U3.

### 6.1 Logic side

```text
GPIO48 -> VEBUS_TX -> D
GPIO47 <- VEBUS_RX <- R
GPIO6  -> VEBUS_DIR -> DE and /RE
```

Target serial rate: 256000 baud. The ISOW1412 is specified for up to 500 kbit/s, providing margin above the target rate.

### 6.2 Bus side

Half-duplex connection:

```text
Y + A -> VEBUS_A
Z + B -> VEBUS_B
```

Place:

- dedicated SM712 footprint on A/B
- 120-Ohm termination footprint + solder jumper, DNI/open by default
- two bias resistor footprints + solder jumpers, DNI/open by default
- `VE_A`, `VE_B`, `VE_VISO`, `VE_GISO` test points

No Modbus field-side copper or reference may cross into the VE.Bus isolated domain.

## 7. VE.Bus STB / PD

GPIO43 and GPIO44 are reserved for STB/PD handling.

Rev-A PCB must route them to isolated-interface footprints and test points, but the active components are **DNI by default** until the target MultiPlus has been characterized.

Required footprint flexibility:

- isolated open-drain/output path for STB
- isolated digital/input-sense path for PD
- series-resistor positions
- pull-up/pull-down positions on the VE.Bus side

This is a population option, not a PCB-layout blocker.

## 8. VE.Bus isolated power path

VE.Bus-derived Core power is a defined Rev-A topology rather than a direct optional buck path.

```text
VEBUS_VPLUS / VEBUS_GND
        |
 fuse / input protection
        |
 isolated DC/DC
        || galvanic barrier
        |
 VE_ISO_5V / CORE_GND
        |
      D_VE
        |
      SYS_5V
```

Rules:

- converter input return is only `VEBUS_GND`
- converter output return is `CORE_GND`
- no direct copper joins `VEBUS_GND` and `CORE_GND`
- `D_VE` blocks reverse current into the converter and VE.Bus
- exact converter input range is selected after measuring `VEBUS_VPLUS` on the target MultiPlus
- reserve board area/footprint strategy for either a compact PCB DC/DC module or a later integrated isolated converter design

The first bench prototype may use a commercial isolated 9-18 V to 5 V or 18-36 V to 5 V module as appropriate after measurement. This module is not frozen as the production part.

## 9. Heltec carrier connections

Mandatory carrier signals:

```text
5V
3V3
GND
GPIO2
GPIO4
GPIO5
GPIO6
GPIO43
GPIO44
GPIO47
GPIO48
```

The remaining Heltec onboard resources stay connected on the module itself.

Do not use GPIO3, GPIO45 or GPIO46 for Rev-A carrier functions because they are ESP32-S3 strapping pins.

## 10. Mechanical / RF rules

- keep the Heltec LoRa antenna region clear according to the module reference design
- do not place switching-regulator magnetics underneath or directly next to either RF antenna region
- put RJ45 and field terminal at board edges
- keep both isolated barriers free of copper on all layers
- provide a clear silkscreen boundary for `MODBUS ISO` and `VEBUS ISO`
- maintain direct access to Heltec USB-C without removing the carrier
- preserve access to USER/PRG button and OLED

## 11. Rev-A BOM - critical parts

| Ref | Function | Preferred part / class | Population |
|---|---|---|---|
| U1 | input eFuse | TPS2660 PWP-class selected suffix | required |
| U2 | external 5-30 V buck | LMR38020 DDA | required |
| U3 | Modbus isolated RS485 + isolated power | ISOW1412 DFM | required |
| U4 | Modbus TVS | SM712 or equivalent | required |
| U5 | VE.Bus isolated RS485 + isolated power | ISOW1412 DFM | required |
| U6 | VE.Bus TVS | SM712 or equivalent | required |
| U7 | VE.Bus isolated DC/DC | input-range-specific isolated 5-V converter/module | selected after VEBUS_VPLUS measurement |
| D_EXT | external source OR | low-loss Schottky/reverse-blocking element | required |
| D_VE | VE.Bus source OR | low-loss Schottky/reverse-blocking element | required with U7 |
| J1 | 4-pin field connector | 5.08-mm pluggable terminal | required |
| J2 | VE.Bus connector | RJ45 8P8C | required |
| R_MB_TERM | Modbus termination | 120 Ohm 1 % | required, jumper selectable |
| R_MB_PU/PD | Modbus bias | 680 Ohm 1 % | required, jumper selectable |
| R_VE_TERM | VE.Bus termination | 120 Ohm 1 % | footprint, DNI by default |
| R_VE_PU/PD | VE.Bus bias | 680 Ohm 1 % | footprint, DNI by default |
| L1 | buck inductor | 15 uH, >=3-A Isat preferred | required |
| STB interface | isolated optional control | suitable isolator/open drain | footprint, DNI |
| PD interface | isolated optional sense | suitable digital isolator/input | footprint, DNI |

Passives use X7R ceramics where practical and voltage derating appropriate for the rail.

## 12. Bring-up sequence

1. Assemble external power section only; Heltec and bus ICs absent.
2. Sweep J1 input from 5 V to 30 V and verify `VPROTECTED` and `EXT_5V`.
3. Verify reverse-polarity protection and current limiting with a current-limited bench supply.
4. Verify `D_EXT` drop and `SYS_5V` under load.
5. Attach Heltec; verify carrier power and USB power independently.
6. Connect USB while carrier supply is active and verify no reverse current into the external branch.
7. Populate/enable Modbus interface; check DE/RE default state and loopback with an isolated USB-RS485 adapter.
8. Verify Modbus at 9.6, 19.2, 115.2 and 256 kbit/s electrical stress test.
9. Populate VE.Bus data interface but begin receive-only on a real MultiPlus.
10. Measure A/B polarity, idle bias, termination and frame timing.
11. Measure `VEBUS_VPLUS`, available current and source impedance before selecting U7.
12. Populate U7/D_VE and verify VE.Bus-powered operation while preserving isolation.
13. Verify simultaneous external + VE.Bus + USB source operation with no back-feed.
14. Measure STB/PD behaviour before populating those optional paths.
15. Perform ESD/EFT pre-compliance checks and thermal soak over the specified external-input range.

## 13. Rev-A freeze rule

Frozen for PCB capture:

- connector functions
- protected 5-30-V external branch
- separate isolated VE.Bus power branch
- post-conversion low-voltage source ORing
- two independent ISOW1412 bus domains
- GPIO allocation
- test-point set
- selectable Modbus termination/bias
- optional VE.Bus termination/bias
- reserved STB/PD interface footprints

Measurement-dependent selections that do not change the topology:

- exact isolated VE.Bus DC/DC input range/part
- final D_EXT/D_VE part after drop/thermal measurement
- exact TPS2660 programming passives
- exact LMR38020 output set point and RT resistor after source-OR loss is finalized

A topology change to any frozen item requires a new hardware revision.
