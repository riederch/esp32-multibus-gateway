# Rev-A power topology

The MultiBus carrier has two independent field power sources:

1. external supply on `J1 V+ / V-`
2. optional Victron supply on `J2 VEBUS_VPLUS / VEBUS_GND`

The two source domains are not tied together on their raw-input side. Each source is converted separately. Source ORing happens only on the Core-side low-voltage rail.

```text
External V+ / V-
      |
 input protection
      |
 non-isolated DC/DC
      |
 EXT_5V
      |
 D_EXT
      +----------------------+
                             |
                          SYS_5V ------ Heltec 5V
                             |
 D_VE                        |
      +----------------------+
      |
 VE_ISO_5V
      |
 isolated DC/DC
      ||  galvanic barrier
      |
VE.Bus V+ / VE.Bus GND
```

## External branch

The external branch retains the protected 5-30 V input concept:

```text
J1 V+
  -> input fuse/PTC option
  -> TVS
  -> TPS2660 protection/eFuse
  -> LMR38020 buck
  -> EXT_5V
  -> D_EXT
  -> SYS_5V

J1 V- -> CORE_GND
```

`D_EXT` is a one-way source-OR element. It may be implemented as a suitable low-loss Schottky diode or another simple reverse-blocking element. The previously considered LM66100/LM66200 source selector is not part of Rev A.

The LMR38020 set point must be selected together with the forward drop of `D_EXT` so that `SYS_5V` remains inside the Heltec 5-V input range over load and input voltage. The former assumption that the pre-OR buck output itself is fixed at 4.75 V is therefore removed.

## VE.Bus branch

The VE.Bus supply path must preserve galvanic isolation from the Core:

```text
J2 pin 2  VEBUS_VPLUS
J2 pin 3  VEBUS_GND
      |
 input protection / fuse
      |
 isolated DC/DC converter
      ||
      || galvanic isolation
      ||
      |
 VE_ISO_5V / CORE_GND
      |
 D_VE
      |
 SYS_5V
```

The isolated converter input is referenced only to `VEBUS_GND`. Its output return is connected to `CORE_GND`. There is no direct copper connection from `VEBUS_GND` to `CORE_GND`.

The exact isolated converter input range is selected after measuring `VEBUS_VPLUS` on the target Victron MultiPlus in all relevant operating states. For bench prototypes, suitable isolated module classes include 9-18 V to 5 V and 18-36 V to 5 V modules. The production converter must provide adequate isolation rating, current capability and efficiency for the measured source.

`D_VE` provides reverse blocking so USB or the external branch cannot drive power back through the isolated converter into VE.Bus.

## USB coexistence

USB-C remains on the Heltec board. `SYS_5V` is the carrier connection to the Heltec 5-V rail.

Both carrier branches contain one-way OR elements. Therefore USB VBUS must not be able to back-feed either the external buck branch or the isolated VE.Bus branch.

The preferred voltage hierarchy is:

```text
USB rail       highest nominal source
SYS_5V carrier slightly below USB under normal load
```

This gives USB natural priority during service/programming without firmware-controlled source switching.

## Isolation domains

The final power architecture has three relevant reference domains:

```text
CORE domain
  CORE_GND
  external J1 V-
  SYS_5V

VE.Bus domain
  VEBUS_GND
  VEBUS_VPLUS

RS485 isolated bus domains
  MB_GISO / MB_VISO
  VE_GISO / VE_VISO
```

`VEBUS_GND` must not be connected directly to `CORE_GND`. Power transfer from VE.Bus to Core crosses only the isolated DC/DC barrier. VE.Bus data crosses only the VE.Bus ISOW1412 barrier.

## Required schematic objects

Rev A shall expose at least these power nets/test points:

- `VIN_FIELD`
- `VPROTECTED`
- `EXT_5V`
- `SYS_5V`
- `CORE_GND`
- `VEBUS_VPLUS`
- `VEBUS_GND`
- `VE_ISO_5V`

The schematic shall contain separate designators for `D_EXT` and `D_VE` so either OR element can be changed independently after voltage-drop and thermal measurements.

## Bring-up checks

1. Verify external branch alone over the complete specified input range.
2. Verify no current flows from `SYS_5V` back into the external converter.
3. Measure VE.Bus V+ before selecting/populating the isolated converter.
4. Verify isolation resistance between `VEBUS_GND` and `CORE_GND` with the unit unpowered.
5. Verify VE.Bus branch alone.
6. Verify no current flows from `SYS_5V` back into VE.Bus.
7. Power both branches simultaneously and verify stable source sharing/selection.
8. Connect USB while either or both carrier sources are active and verify no back-feed.
9. Measure `SYS_5V` minimum/maximum under worst-case load and temperature.
10. Measure OR-element voltage drop and thermal rise at maximum expected current.
