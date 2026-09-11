# KiCad Rev-A schematic

This directory contains the first electrical schematic capture for the MultiBus Gateway Rev A carrier.

## Files

- `multibus-rev-a.sch` - KiCad legacy schematic, importable by current KiCad versions
- `multibus-rev-a.lib` - local custom symbols for the Heltec carrier, TPS2660, LMR38020, ISOW1412 and isolated VE.Bus DC/DC module
- `sym-lib-table` - local symbol-library registration

## Scope captured

The schematic contains the frozen Rev-A topology:

- J1 field connector: external supply plus isolated Modbus A/B
- TPS2660 protected external power input
- LMR38020 external DC/DC stage
- post-conversion `D_EXT` / `D_VE` low-voltage source ORing
- galvanically isolated VE.Bus power converter placeholder selected after `VEBUS_VPLUS` measurement
- Heltec V4.2 carrier connections
- dedicated ISOW1412 Modbus interface
- dedicated ISOW1412 VE.Bus interface
- fixed UART/DIR GPIO assignments
- VE.Bus STB/PD as DNI interface requirements
- required power, isolation and UART test nets

## Important capture status

This is an electrical capture baseline, not yet a PCB-release artifact.

The execution environment used to generate it does not contain `kicad-cli`, so it has not yet passed KiCad ERC or automated schematic conversion. On first opening in a current KiCad version:

1. import/convert the legacy schematic to `.kicad_sch`
2. confirm all custom symbol pins against the selected manufacturer package suffix
3. split the design into the planned hierarchical sheets if desired
4. add/finalize all programming passives around TPS2660 and LMR38020
5. select the exact `D_EXT`/`D_VE` devices after voltage-drop/current calculation
6. select the isolated VE.Bus DC/DC module after measuring `VEBUS_VPLUS`
7. add the final SM712, termination/bias jumpers, decoupling, ferrites and test-point symbols
8. assign footprints
9. run ERC with zero unexplained errors before PCB layout

## Source of truth

Electrical intent is defined by:

- `docs/hardware-rev-a.md`
- `docs/power-topology-rev-a.md`
- `docs/hardware.md`

If the KiCad capture and those documents disagree, resolve the discrepancy before PCB release rather than silently choosing one.
