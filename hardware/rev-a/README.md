# KiCad Rev-A schematic

This directory contains the Rev-A electrical capture for the MultiBus Gateway carrier around the Heltec HTIT-WB32LAF V4.2.

## Files

- `multibus-rev-a.sch` - initial KiCad legacy capture baseline
- `multibus-rev-a-detail.sch` - current detailed electrical working schematic; use this for KiCad import/conversion and ERC
- `multibus-rev-a.lib` - local custom symbols for the Heltec carrier, TPS2660, LMR38020, ISOW1412 and isolated VE.Bus DC/DC module
- `sym-lib-table` - local symbol-library registration
- `bom-critical.csv` - current critical component/value/population table

## Current electrical scope

The detailed schematic contains:

- J1 field connector: external 5-30 V supply plus isolated Modbus A/B
- fuse/PTC and 33-V-class TVS input protection
- TPS26600 protected/reverse-polarity input stage
- LMR38020 400-kHz buck stage with explicit RT, feedback, bootstrap, inductor and output capacitors
- independent `D_EXT` / `D_VE` low-voltage source ORing
- galvanically isolated VE.Bus power converter placeholder selected after `VEBUS_VPLUS` measurement
- `JP_USB_SAFE` physical disconnect between `SYS_5V` and the Heltec 5-V input
- Heltec V4.2 carrier GPIO connections
- dedicated ISOW1412 Modbus interface
- dedicated ISOW1412 VE.Bus interface
- fixed UART/DIR GPIO assignments
- Modbus TVS, termination and bias requirements
- VE.Bus TVS and optional termination/bias requirements
- VE.Bus STB/PD as DNI interface requirements
- power/isolation/UART test nets

## Frozen baseline values

```text
TPS26600PWP
  R_ILIM      11.8 kOhm 1 %
  C_dVdT      22 nF
  UVLO        tied to IN
  OVP         tied to RTN
  MODE        tied to RTN

LMR38020SDDAR
  fSW         400 kHz
  R_RT        64.9 kOhm 1 %
  R_FBT       100 kOhm 1 %
  R_FBB       23.7 kOhm 1 %
  L           15 uH, Isat >= 3 A
  C_BOOT      100 nF
  COUT        3 x 22 uF X7R

ISOW1412DFM x2
  VIO         3V3
  VDD         SYS_5V
  MODE        VISOOUT for 5-V isolated side
  EN/FLT      4.7-kOhm pull-up to 3V3
```

`U1_RTN` is not `CORE_GND`; preserving this distinction is required for TPS26600 reverse-polarity protection.

## USB service rule

Heltec V4.2 must not be powered simultaneously from USB and its external 5-V pin.

```text
Normal field operation:  JP_USB_SAFE CLOSED
USB service/programming: JP_USB_SAFE OPEN before USB-C connection
```

The PCB silkscreen must label this clearly as `OPEN FOR USB`.

## Still measurement-dependent

- exact isolated VE.Bus DC/DC input range/part after measuring `VEBUS_VPLUS`
- exact VE.Bus power-input TVS/fuse coordination
- final `D_EXT`/`D_VE` verification for forward drop and thermal margin
- VE.Bus A/B polarity and network bias/termination
- STB/PD electrical implementation

## KiCad release workflow

The execution environment used to generate these files does not contain `kicad-cli`, so the schematics have not yet passed KiCad ERC.

Before PCB routing:

1. open/import `multibus-rev-a-detail.sch` in the target KiCad version
2. convert/save to current `.kicad_sch`
3. verify every custom symbol pin against the exact package datasheet
4. assign footprints
5. split into the planned hierarchical sheets if useful
6. run ERC with zero unexplained errors
7. generate/review BOM and netlist
8. preserve all isolation barriers during PCB layout

## Source of truth

Electrical intent is defined by:

- `docs/hardware.md`
- `docs/hardware-rev-a.md`
- `docs/power-topology-rev-a.md`
- `hardware/rev-a/bom-critical.csv`

If the schematic and these documents disagree, resolve the discrepancy before PCB release.
