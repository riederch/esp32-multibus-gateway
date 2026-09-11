# Rev-A power topology

The MultiBus carrier has two independent field power sources:

1. external supply on `J1 V+ / V-`
2. optional Victron supply on `J2 VEBUS_VPLUS / VEBUS_GND`

The two raw source domains are never tied together. Each source is converted separately. Source ORing happens only on the Core-side low-voltage rail.

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
                          SYS_5V
                             |
                        JP_USB_SAFE
                             |
                         Heltec 5V

VE.Bus V+ / VE.Bus GND
      |
 input protection
      |
 isolated DC/DC
      || galvanic barrier
      |
 VE_ISO_5V
      |
 D_VE
      +----------------------> SYS_5V
```

## External branch

```text
J1 V+
  -> fuse/PTC
  -> TVS
  -> TPS26600PWP
  -> LMR38020SDDAR
  -> EXT_5V
  -> D_EXT
  -> SYS_5V

J1 V- -> CORE_GND
```

TPS26600 baseline:

```text
R_ILIM = 11.8 kOhm 1 %
C_dVdT = 22 nF
UVLO   = IN
OVP    = RTN
MODE   = RTN
```

The TPS26600 `RTN` node and PowerPAD are part of the reverse-polarity protection path and are not hard-wired to `CORE_GND`.

LMR38020 baseline:

```text
fSW    = 400 kHz
R_RT   = 64.9 kOhm 1 %
R_FBT  = 100 kOhm 1 %
R_FBB  = 23.7 kOhm 1 %
L      = 15 uH
C_BOOT = 100 nF
COUT   = 3 x 22 uF
```

The nominal `EXT_5V` target is approximately 5.2 V so the drop across `D_EXT` still leaves a valid system rail.

## VE.Bus branch

```text
J2 pin 2 VEBUS_VPLUS
J2 pin 3 VEBUS_GND
      |
 fuse / protection
      |
 isolated DC/DC
      || galvanic isolation
      |
 VE_ISO_5V / CORE_GND
      |
 D_VE
      |
 SYS_5V
```

The isolated converter input is referenced only to `VEBUS_GND`. Its output return is `CORE_GND`. There is no direct copper connection between `VEBUS_GND` and `CORE_GND`.

The exact isolated converter input range is selected after measuring `VEBUS_VPLUS` and available current on the target MultiPlus. Prototype module classes may be 9-18 V to 5 V or 18-36 V to 5 V.

## Source ORing

`D_EXT` and `D_VE` are independent one-way source-OR elements. The current preferred candidate is `PMEG3050EP` or an equivalent low-Vf Schottky with sufficient current margin.

Required behavior:

- external source cannot feed back into VE.Bus branch
- VE.Bus source cannot feed back into external branch
- both carrier sources may be present simultaneously
- `SYS_5V` remains inside the Heltec 5-V-input range under all valid load/source combinations

## USB service disconnect

The Heltec V4.2 documentation does not permit simultaneous powering from USB and the external 5-V pin. Rev A therefore uses a physical disconnect:

```text
SYS_5V -> JP_USB_SAFE -> HELTEC_5V
```

Operating rule:

```text
Field operation       JP_USB_SAFE CLOSED
USB service/program   JP_USB_SAFE OPEN before connecting USB-C
```

The PCB silkscreen must mark the jumper `OPEN FOR USB`.

This is intentionally stronger than relying on source voltage priority or diode back-feed behavior.

## Isolation domains

```text
CORE domain
  CORE_GND
  J1 V-
  EXT_5V
  VE_ISO_5V output
  SYS_5V

VE.Bus source domain
  VEBUS_GND
  VEBUS_VPLUS

Modbus isolated bus domain
  MB_GISO
  MB_VISO

VE.Bus data isolated bus domain
  VE_GISO
  VE_VISO
```

VE.Bus power crosses only the isolated DC/DC barrier. VE.Bus data crosses only the VE.Bus ISOW1412 barrier.

## Required power test points

- `VIN_FIELD`
- `VPROTECTED`
- `EXT_5V`
- `SYS_5V`
- `HELTEC_5V`
- `CORE_GND`
- `VEBUS_VPLUS`
- `VEBUS_GND`
- `VE_ISO_5V`

## Bring-up checks

1. Verify the external branch over 5-30 V.
2. Verify TPS26600 reverse-polarity behavior and current limiting.
3. Verify `EXT_5V` and post-`D_EXT` `SYS_5V` under load.
4. Verify no reverse current into the external branch.
5. Measure VE.Bus V+ and available current before selecting U7.
6. Verify isolation resistance between `VEBUS_GND` and `CORE_GND` unpowered.
7. Populate and test the VE.Bus isolated converter.
8. Verify no reverse current into VE.Bus.
9. Power external and VE.Bus branches simultaneously and verify stable `SYS_5V`.
10. Open `JP_USB_SAFE`, connect USB-C, and verify the Heltec is powered only by USB.
11. With `JP_USB_SAFE` closed, verify normal carrier-powered operation without USB attached.
12. Measure source-OR voltage drop and thermal rise at maximum expected load.
