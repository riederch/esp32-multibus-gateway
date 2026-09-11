# KiCad Rev-A hardware

This directory contains the Rev-A electrical and PCB preparation work for the MultiBus Gateway carrier around the Heltec HTIT-WB32LAF V4.2.

## Files

- `multibus-rev-a.sch` - initial KiCad legacy capture baseline
- `multibus-rev-a-detail.sch` - current detailed electrical working schematic; use this for KiCad import/conversion and ERC
- `multibus-rev-a.lib` - local custom symbols
- `sym-lib-table` - local symbol-library registration
- `fp-lib-table` - local footprint-library registration
- `bom-critical.csv` - current critical component/value/population table
- `pcb-footprints.md` - verified mechanical/footprint baseline and PCB rules
- `multibus-rev-a-placement.kicad_pcb` - first placement study with temporary critical-IC placeholders
- `multibus-rev-a-placement-v2.kicad_pcb` - current placement study using real TI land patterns for U1/U2/U3/U5
- `multibus-rev-a.pretty/` - local footprints

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
- Modbus/VE.Bus protection, termination and bias options
- VE.Bus STB/PD DNI footprints
- required power/isolation/UART test nets

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

## Verified critical land patterns

The current footprint library contains explicit manufacturer-based land patterns for:

```text
U1  TPS26600PWP      TI PWP0016A
U2  LMR38020SDDAR    TI DDA0008B
U3  ISOW1412DFM      TI DFM0020A
U5  ISOW1412DFM      TI DFM0020A
```

Signal-pad dimensions/pitch follow the TI example board layouts. The PowerPAD footprints keep the larger TI thermal copper area distinct from the smaller exposed solder-mask/paste area.

`multibus-rev-a-placement-v2.kicad_pcb` uses these land patterns instead of the temporary placement boxes from the first study.

The board outline in both placement studies is still a non-frozen working size of 110 x 70 mm.

## USB service rule

Heltec V4.2 must not be powered simultaneously from USB and its external 5-V pin.

```text
Normal field operation:  JP_USB_SAFE CLOSED
USB service/programming: JP_USB_SAFE OPEN before USB-C connection
```

Silkscreen: `OPEN FOR USB`.

## Still measurement-dependent

- exact isolated VE.Bus DC/DC input range/part after measuring `VEBUS_VPLUS`
- exact VE.Bus power-input TVS/fuse coordination
- final `D_EXT`/`D_VE` verification for forward drop and thermal margin
- VE.Bus A/B polarity and network bias/termination
- STB/PD electrical implementation
- final enclosure-driven board outline and mounting holes

## KiCad release workflow

The execution environment used to generate these files does not contain `kicad-cli`; ERC/DRC have therefore not been executed here.

Before routing/release:

1. open/import `multibus-rev-a-detail.sch` in the target KiCad version
2. save as current `.kicad_sch`
3. replace the compact Heltec logical symbol with the verified 36-pin physical symbol for netlisting
4. assign the verified footprints, including the TI footprints already present in `multibus-rev-a.pretty`
5. run ERC with zero unexplained errors
6. update PCB from schematic into `multibus-rev-a-placement-v2.kicad_pcb` or a clean successor
7. route while preserving all isolation and RF keep-outs
8. run DRC and manually inspect both isolation barriers
9. generate and review Gerber/drill/BOM/position outputs before ordering

## Source of truth

Electrical intent is defined by:

- `docs/hardware.md`
- `docs/hardware-rev-a.md`
- `docs/power-topology-rev-a.md`
- `hardware/rev-a/bom-critical.csv`
- `hardware/rev-a/pcb-footprints.md`

If the schematic, PCB study and these documents disagree, resolve the discrepancy before PCB release.
