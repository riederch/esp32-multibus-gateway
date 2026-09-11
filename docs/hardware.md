# Hardware

## Reference platform

Reference board:

- Heltec HTIT-WB32LAF V4.2 / WiFi LoRa 32 V4.2
- ESP32-S3
- SX1262 EU868
- onboard OLED
- Wi-Fi / BLE
- native USB-C
- BAT / SOL support
- external GNSS connector

The Rev-A carrier adds protected external field power, optional isolated VE.Bus-derived power, one isolated Modbus RS485 interface and one independent isolated Victron VE.Bus interface.

## Rev-A electrical architecture

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
      ||
      || galvanic isolation
      ||
      |
VE.Bus V+ / VE.Bus GND
```

The two raw source domains are never directly tied together. Source ORing happens only on the Core-side low-voltage rail.

`V-` is the external supply/Core return. `VEBUS_GND` remains galvanically isolated from `CORE_GND`; VE.Bus-derived power crosses an isolated DC/DC converter before joining `SYS_5V`.

## Field terminal

```text
V+   external 5-30 V DC input
V-   external supply return / Core power reference
A    isolated Modbus RS485 A
B    isolated Modbus RS485 B
```

The Modbus transceiver bus-side ground remains floating. A separate Modbus COM terminal is not required for Rev A; test pads expose the isolated reference for commissioning and EMC measurements.

## External power branch

Rev A uses:

1. **TPS2660** industrial eFuse as the protected 5-30 V input stage.
2. **LMR38020** synchronous buck converter.
3. A one-way low-loss OR element `D_EXT` between `EXT_5V` and `SYS_5V`.

The exact buck output set point is chosen together with the forward drop of `D_EXT` so that `SYS_5V` remains inside the Heltec 5-V input range. The previous fixed 4.75-V pre-OR rail assumption is removed.

### Input protection

TPS2660 requirements:

- reverse-polarity protection
- reverse-current blocking
- current limit
- thermal shutdown
- surge/overvoltage protection
- input range covering 5-30 V

Rev-A target current limit is **1 A**. Use the datasheet value close to **11.8 kOhm** for `RILIM`.

UVLO must not use the TPS2660 internal 15-V default because the product supports 5-V input. Configure the UVLO pin so operation down to the specified input range is permitted. OVP is configured above the normal 30-V operating limit while remaining inside downstream component ratings.

Place a field-input TVS, bulk capacitor and local ceramic bypassing directly behind the connector/protection stage.

## VE.Bus optional power branch

VE.Bus pin 2 and pin 3 feed a dedicated galvanically isolated DC/DC converter:

```text
VEBUS_VPLUS / VEBUS_GND
        |
   input protection
        |
 isolated DC/DC
        ||
        || isolation barrier
        ||
        |
    VE_ISO_5V
        |
       D_VE
        |
      SYS_5V
```

The isolated converter input is referenced only to `VEBUS_GND`. Its output return is `CORE_GND`.

The exact input range/type of the isolated converter is selected after measuring VE.Bus V+ on the target MultiPlus across its relevant operating states. Prototype options may use suitable 9-18 V -> 5 V or 18-36 V -> 5 V isolated modules depending on the measured source.

## USB-C coexistence

The Heltec board carries native USB-C. Both carrier power branches are one-way into `SYS_5V`, so USB VBUS must not back-feed either branch.

The preferred source hierarchy is:

```text
USB VBUS       highest nominal source
SYS_5V carrier slightly below USB under normal load
```

This gives USB natural priority during service/programming without firmware-controlled source switching.

## Modbus RS485

Rev A uses **ISOW1412** as the isolated transceiver because it provides:

- reinforced galvanic isolation
- integrated isolated bus-side power
- 500-kbit/s data rate, sufficient for all supported Modbus rates
- 3.3-V-compatible logic-side interface
- receiver fail-safe behaviour

Half-duplex wiring:

```text
ESP32 TX ---- D
ESP32 DIR --- DE and /RE control
ESP32 RX <--- R

ISOW1412 Y --+
             +---- RS485 A
ISOW1412 A --+

ISOW1412 Z --+
             +---- RS485 B
ISOW1412 B --+
```

The paired full-duplex line pins are strapped according to the manufacturer's half-duplex connection guidance.

### Modbus protection and termination

- SM712 or equivalent RS485 TVS at the connector
- optional 120-ohm termination across A/B via solder jumper
- optional fail-safe bias footprint on the isolated bus side
- default Modbus-master population: 680-ohm pull-up to isolated bus supply and 680-ohm pull-down to isolated bus ground, enabled via solder jumpers
- test pads: `MB_A`, `MB_B`, `MB_GISO`, `MB_VISO`, `MB_TX`, `MB_RX`, `MB_DIR`

Bias resistors must be disabled if another node already provides network bias.

## Victron VE.Bus

VE.Bus has an independent **ISOW1412** transceiver. It never shares the Modbus transceiver or isolated power domain.

Target baud rate: **256000 baud**.

RJ45 assignment for Rev A:

| Pin | Signal | Rev-A connection |
|---:|---|---|
| 1 | NC | open |
| 2 | V+ | optional isolated gateway-power input |
| 3 | GND | VE.Bus reference / isolated-power input return |
| 4 | A | isolated VE.Bus transceiver A |
| 5 | B | isolated VE.Bus transceiver B |
| 6 | STB | optional isolated control footprint, DNI |
| 7 | PD | optional isolated control/sense footprint, DNI |
| 8 | NC | open |

### VE.Bus termination/bias

Do not populate additional termination or bias by default. Provide footprints and solder-jumper options only. The production values are determined from measurement on the target MultiPlus network.

## GPIO allocation

Rev-A carrier assignment:

```text
Modbus RX      GPIO2
Modbus TX      GPIO4
Modbus DIR     GPIO5

VE.Bus RX      GPIO47
VE.Bus TX      GPIO48
VE.Bus DIR     GPIO6

VE.Bus STB     GPIO43   optional / DNI interface
VE.Bus PD      GPIO44   optional / DNI interface
```

GPIO3, GPIO45 and GPIO46 are intentionally not used because they are ESP32-S3 strapping pins. GPIO26 remains reserved as routing/revision margin.

The complete board mapping is defined in `include/board/BoardPins.h`.

## Existing Heltec resources

```text
GPIO0   USER / PRG
GPIO1   battery ADC
GPIO35  LED
GPIO36  Vext
GPIO37  ADC control
GPIO17  OLED SDA
GPIO18  OLED SCL
GPIO21  OLED reset
GPIO34  GNSS power
GPIO38  GNSS RX
GPIO39  GNSS TX
GPIO40  GNSS wake
GPIO41  GNSS PPS
GPIO42  GNSS reset
GPIO7   LoRa RF/FEM control
GPIO8   LoRa NSS
GPIO9   LoRa SCK
GPIO10  LoRa MOSI
GPIO11  LoRa MISO
GPIO12  LoRa reset
GPIO13  LoRa busy
GPIO14  LoRa DIO1
GPIO19  USB D-
GPIO20  USB D+
```

## GNSS

Use the Heltec external GNSS interface. Rev A does not duplicate the GNSS level shifting or power-control circuitry already provided by the reference board.

The preferred initial module is the same electrical class as the Heltec expansion option; final receiver selection does not affect carrier routing.

## PCB layout rules

- 4-layer PCB preferred.
- Keep switching-regulator hot loops compact and away from LoRa/GNSS antennas.
- Maintain all isolation-barrier clearance with no copper pour crossing the barrier.
- Keep Modbus and VE.Bus isolated domains physically separated.
- Place TVS devices at their connectors before long PCB traces.
- Route RS485 A/B as coupled differential pairs with no stubs except very short protection/test-point branches.
- Keep the LoRa antenna keep-out from the Heltec reference design completely free of copper, enclosure metal and tall components.
- Keep GNSS antenna/feed away from switching-node copper.
- Provide accessible test points for all supply rails and both UARTs.

## Required Rev-A test points

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

## Hardware freeze status

The Rev-A schematic topology, interfaces, GPIO allocation and default population are fixed.

Items that remain to be measured during bring-up are validation parameters rather than architecture blockers:

- actual VE.Bus A/B polarity and idle levels
- VE.Bus native termination/bias behaviour
- VE.Bus V+ voltage/current availability and resulting isolated DC/DC module/input range
- STB/PD electrical behaviour
- EMC/surge performance of the assembled PCB
- maximum thermal rise and SYS_5V voltage margin under worst-case load

See `docs/hardware-rev-a.md` and `docs/power-topology-rev-a.md` for the schematic-capture specification.