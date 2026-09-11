# Rev-A placement study

This file documents the first KiCad PCB placement study for the MultiBus Gateway Rev A carrier.

## Status

The placement study is intentionally **not** a routed or DRC-released PCB. It exists to validate mechanical partitioning before schematic import/ERC and final manufacturer land-pattern assignment.

Current working board outline:

```text
110 mm x 70 mm
```

This outline is a working dimension only and is not frozen until the enclosure/mounting concept is selected.

## Files

- `multibus-rev-a-placement.kicad_pcb` - KiCad PCB placement study
- `multibus-rev-a.pretty/Heltec_WiFi_LoRa_32_V4_2.kicad_mod` - Heltec carrier footprint
- `multibus-rev-a.pretty/Phoenix_1757268_MSTBA_2.5_4_G_5.08.kicad_mod` - J1 field connector footprint baseline
- `multibus-rev-a.pretty/Wuerth_615008143721.kicad_mod` - J2 VE.Bus RJ45 footprint baseline
- `multibus-rev-a.pretty/ISO_DC_DC_UNIVERSAL_4PIN.kicad_mod` - prototype-only isolated DC/DC interface/reservation

## Placement concept

```text
LEFT / FIELD                                               RIGHT / VE.BUS

J1  Phoenix 4-pos                              J2 Wuerth RJ45
 |                                                  |
TPS26600                                           VE.Bus TVS
 |                                                  |
LMR38020                                      ISOW1412 #2
 |                                                  |
Modbus protection / ISOW1412 #1              isolated U7 reserve
          \                                      /
           +--------- Heltec V4.2 ---------------+
                        |
                   JP_USB_SAFE
```

The Heltec is central because it dominates carrier mechanics and provides the USB service interface. J1 and J2 remain on opposite board edges to reduce accidental field/VE.Bus crossover and to keep the two isolated domains visually obvious.

## Isolation zoning

The placement file marks three working zones:

1. Modbus isolated domain around U3 and J1 A/B protection
2. VE.Bus data isolated domain around U5 and J2 A/B protection
3. VE.Bus isolated-power domain around U7

These drawn zones are placement guides, not substitutes for final creepage/clearance checks. No copper plane or trace may cross the corresponding isolation barriers in the routed board.

## U7 strategy

The actual VE.Bus-powered isolated DC/DC converter is still measurement-dependent. Therefore Rev A uses a prototype reservation rather than pretending an exact production footprint is known.

`ISO_DC_DC_UNIVERSAL_4PIN` provides:

```text
pin 1  VE input +
pin 2  VE input - / VEBUS_GND
pin 3  Core output - / CORE_GND
pin 4  Core output + / VE_ISO_5V
```

The footprint reserves approximately 36 x 26 mm. It is marked `PROTOTYPE ONLY` and must be replaced by the selected converter manufacturer's land pattern before production release.

## Connector verification

J1 is based on Phoenix Contact `1757268` / MSTBA 2,5/4-G-5,08 and uses 5.08-mm pin spacing.

J2 is based on Wuerth Elektronik `615008143721` / WR-MJ 8P8C right-angle unshielded. The footprint follows the manufacturer's staggered 1.27-mm signal-pin pattern and two 3.25-mm mechanical holes. Before production release, compare the committed footprint against the current Wuerth KiCad design kit and current datasheet revision.

## Deliberately unresolved in this placement file

The following footprints are still represented by placement envelopes only:

- TPS26600PWP
- LMR38020SDDAR
- ISOW1412DFM x2
- SM712 x2 and surrounding passives
- switching inductor and converter passives
- all test points
- STB/PD optional isolation components

Do not route from this study as though those envelopes were final land patterns.

## Next gate

Before actual routing:

1. open/convert `multibus-rev-a-detail.sch` in a current KiCad version
2. replace the compact Heltec logical symbol with `HELTEC_V4_2_PHYSICAL` in the netlisted design
3. assign exact manufacturer land patterns for U1/U2/U3/U5 and all critical parts
4. import/update the PCB from the schematic
5. preserve the placement concept unless ERC/net connectivity exposes a reason to change it
6. run ERC and then DRC with no unexplained errors
7. freeze the board outline only after enclosure/mounting requirements are known
