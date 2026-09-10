# Hardware Rev A

This document is the schematic-capture specification for the first MultiBus carrier PCB around the Heltec HTIT-WB32LAF V4.2.

## 1. Functional blocks

```text
J1 FIELD INPUT
V+ / V- / A / B
   |
   +-- protected power --> 4.75 V --> Heltec 5V
   |
   `-- isolated Modbus A/B --> ESP32-S3 UART

J2 VE.BUS RJ45
   |
   +-- isolated VE.Bus A/B --> ESP32-S3 UART
   |
   +-- STB/PD optional isolated footprints
   `-- V+ optional power path, DNI
```

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

Default design values:

```text
RILIM = 11.8 kOhm, 1 %    target approximately 1 A current limit
UVLO  = external configuration allowing operation from 5 V
OVP   = threshold above 30 V but below downstream absolute maximum
```

Do not use the device's internal 15-V UVLO mode.

Provide footprints for:

- resettable fuse or replaceable fuse at J1
- bidirectional input TVS sized for the selected OVP strategy
- 100 nF / 50 V ceramic
- 4.7 uF / 50 V ceramic
- 47-100 uF bulk capacitor after protection
- `VIN_FIELD` and `VPROTECTED` test points

### 4.2 Buck converter

U2: TI LMR38020.

Target:

```text
VIN   = VPROTECTED
VOUT  = 4.75 V nominal
fSW   = 400 kHz
IOUT  = design for 2 A peak capability
```

Reference values:

```text
RFBT  = 100 kOhm, 1 %
RFBB  = 26.7 kOhm, 1 %
RT    = 64.9 kOhm, 1 %
L1    = 15 uH, Isat >= 3 A preferred, low DCR
CBOOT = 100 nF
COUT  = 3 x 22 uF X7R, >= 10 V
CIN   = 4.7 uF / 50 V + 100 nF directly at VIN/GND
```

Follow the LMR38020 layout example for the VIN-SW-inductor-output current loop.

### 4.3 Heltec supply isolation from USB

Insert a reverse-current-blocking ideal-diode/load-switch stage between `VBUCK_4V75` and the Heltec 5-V pin.

Required behaviour:

- current from carrier to Heltec is allowed
- current from Heltec/USB back into the buck converter is blocked
- USB at nominal 5 V naturally overrides the 4.75-V carrier rail
- at least 1 A continuous capability, 2 A transient target

Candidate class: LM66100 or equivalent reverse-blocking ideal-diode device. Final footprint choice may follow distributor availability without changing the topology.

## 5. Modbus isolated RS485

U3: ISOW1412.

### 5.1 Logic side

```text
GPIO4  -> MODBUS_TX -> D
GPIO2  <- MODBUS_RX <- R
GPIO5  -> MODBUS_DIR -> DE and /RE control logic
3V3    -> VIO / logic supply as required by the selected ISOW1412 configuration
CORE_GND -> logic-side ground
```

Implement deterministic TX/RX direction control. At reset, driver must default disabled and receiver enabled.

### 5.2 Isolated bus side

Configure the full-duplex line pins as half duplex according to TI guidance:

```text
Y + A -> MODBUS_A
Z + B -> MODBUS_B
```

Do not connect `MB_GISO` to `CORE_GND` or `V-`.

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
GPIO48 -> VEBUS_TX
GPIO47 <- VEBUS_RX
GPIO6  -> VEBUS_DIR
```

Target serial rate: 256000 baud.

### 6.2 Bus side

Half-duplex connection identical in principle to Modbus:

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

## 8. VE.Bus V+ optional supply

Route RJ45 pin 2 only to:

```text
VEBUS_VPLUS test point
optional fuse/protection footprint
optional regulator-input footprint
```

Default Rev-A assembly leaves this path electrically disconnected from the Core supply.

Reserve enough PCB area for a second high-voltage buck/ideal-diode path. The intended topology is the same class as the field-input path, but it is populated only after actual V+ voltage and available current have been measured.

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
| U1 | input eFuse | TPS2660 | required |
| U2 | 5-30 V buck | LMR38020 | required |
| U3 | Modbus isolated RS485 + isolated power | ISOW1412 | required |
| U4 | Modbus TVS | SM712 or equivalent | required |
| U5 | VE.Bus isolated RS485 + isolated power | ISOW1412 | required |
| U6 | VE.Bus TVS | SM712 or equivalent | required |
| U7 | 4.75-V reverse blocker | LM66100 class or equivalent | required |
| J1 | 4-pin field connector | 5.08-mm pluggable terminal | required |
| J2 | VE.Bus connector | RJ45 8P8C | required |
| R_MB_TERM | Modbus termination | 120 Ohm 1 % | required, jumper selectable |
| R_MB_PU/PD | Modbus bias | 680 Ohm 1 % | required, jumper selectable |
| R_VE_TERM | VE.Bus termination | 120 Ohm 1 % | footprint, DNI by default |
| R_VE_PU/PD | VE.Bus bias | 680 Ohm 1 % | footprint, DNI by default |
| L1 | buck inductor | 15 uH, >=3-A Isat | required |
| STB interface | isolated optional control | suitable isolator/open drain | footprint, DNI |
| PD interface | isolated optional sense | suitable digital isolator/input | footprint, DNI |
| VE power path | optional VE.Bus-derived power | protected HV buck + reverse block | footprint/area reserved, DNI |

Passives use X7R ceramics where practical and voltage derating appropriate for the rail.

## 12. Bring-up sequence

1. Assemble power section only; Heltec and bus ICs absent.
2. Sweep J1 input from 5 V to 30 V and verify `VPROTECTED` and 4.75-V rail.
3. Verify reverse-polarity protection and current limiting with a current-limited bench supply.
4. Attach Heltec; verify carrier power and USB power independently.
5. Connect USB while carrier supply is active and verify no reverse current into either source.
6. Populate/enable Modbus interface; check DE/RE default state and loopback with an isolated USB-RS485 adapter.
7. Verify Modbus at 9.6, 19.2, 115.2 and 256 kbit/s electrical stress test.
8. Populate VE.Bus interface but begin receive-only on a real MultiPlus.
9. Measure A/B polarity, idle bias, termination and frame timing.
10. Only after passive measurements, enable VE.Bus transmission.
11. Measure RJ45 V+ and STB/PD behaviour before populating those optional paths.
12. Perform ESD/EFT pre-compliance checks and thermal soak at 5-V and 30-V input.

## 13. Rev-A freeze rule

The following are frozen for PCB capture:

- connector functions
- protected 5-30-V field input
- 4.75-V carrier rail
- two independent ISOW1412 bus domains
- GPIO allocation
- test-point set
- selectable Modbus termination/bias
- optional VE.Bus termination/bias
- reserved STB/PD interface footprints
- reserved VE.Bus V+ power option

A change to any frozen item requires a new hardware revision.