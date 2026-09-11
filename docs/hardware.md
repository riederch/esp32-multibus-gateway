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
      || galvanic isolation
      |
 VE_ISO_5V
      |
 D_VE
      +----------------------> SYS_5V
```

The two raw source domains are never directly tied together. Source ORing happens only after conversion to the Core-side low-voltage rail.

`V-` is the external supply/Core return. `VEBUS_GND` remains galvanically isolated from `CORE_GND`; VE.Bus-derived power crosses an isolated DC/DC converter before joining `SYS_5V`.

## Field terminal

```text
V+   external 5-30 V DC input
V-   external supply return / Core power reference
A    isolated Modbus RS485 A
B    isolated Modbus RS485 B
```

The Modbus transceiver bus-side ground remains floating. Test pads expose the isolated reference for commissioning and EMC measurements.

## External power branch

Rev A uses:

1. **TPS26600PWP** industrial eFuse/protection stage.
2. **LMR38020SDDAR** synchronous buck converter.
3. **D_EXT** low-loss Schottky source-OR element.
4. **JP_USB_SAFE** physical disconnect between `SYS_5V` and the Heltec 5-V pin.

### TPS26600 input protection

Design values:

```text
input range   5-30 V specified
R_ILIM        11.8 kOhm, 1 %
C_dVdT        22 nF
UVLO          tied to IN for operation below the internal 15-V default
OVP           tied to RTN for Rev-A baseline; surge clamping remains external
MODE          tied to RTN
SHDN          enabled from input rail
```

`RTN` is the TPS26600 reverse-polarity return and **must not be hard-tied to CORE_GND**. The PowerPAD belongs to the RTN plane.

Input protection includes a fuse/PTC footprint, a 33-V-class bidirectional TVS candidate, bulk capacitance and local ceramic bypassing. Final surge coordination is validated during pre-compliance testing.

### LMR38020 buck

Target:

```text
VIN           VPROTECTED
fSW           400 kHz
R_RT          64.9 kOhm, 1 %
L1            15 uH, Isat >= 3 A
C_BOOT        100 nF
R_FBT         100 kOhm, 1 %
R_FBB         23.7 kOhm, 1 %
EXT_5V        approximately 5.2 V nominal
COUT          3 x 22 uF X7R >= 10 V
CIN           4.7 uF / 50 V + 100 nF close to VIN/GND
```

`EXT_5V` is intentionally above 5.0 V so the selected Schottky drop still leaves a valid `SYS_5V` rail. Final `SYS_5V` minimum/maximum is a bring-up measurement.

## Source ORing

```text
EXT_5V    -> D_EXT --+
                     +--> SYS_5V
VE_ISO_5V -> D_VE ---+
```

Rev A uses independent Schottky source-OR elements. `PMEG3050EP` is the current preferred class for both positions. The final decision remains subject to voltage-drop and thermal measurements at maximum load.

## USB-C service isolation

Heltec V4.2 documentation states that USB and the external 5-V pin are not to be powered simultaneously. Rev A therefore does **not** rely on voltage priority or passive back-feed assumptions.

`JP_USB_SAFE` physically disconnects `SYS_5V` from the Heltec 5-V pin.

```text
Normal field operation:  JP_USB_SAFE CLOSED
USB service/programming: JP_USB_SAFE OPEN before USB-C is connected
```

Silkscreen must clearly state `OPEN FOR USB` next to the jumper.

External and VE.Bus carrier sources may still be present simultaneously because their low-voltage outputs are separately ORed before `SYS_5V`.

## VE.Bus isolated power branch

VE.Bus pin 2 and pin 3 feed a dedicated isolated DC/DC converter:

```text
VEBUS_VPLUS / VEBUS_GND
        |
 fuse / protection
        |
 isolated DC/DC
        || isolation barrier
        |
    VE_ISO_5V / CORE_GND
        |
       D_VE
        |
      SYS_5V
```

The isolated converter input is referenced only to `VEBUS_GND`. Its output return is `CORE_GND`. The exact converter input range is selected after measuring `VEBUS_VPLUS` and available current on the target MultiPlus.

Prototype module classes may be 9-18 V -> 5 V or 18-36 V -> 5 V, depending on the measured VE.Bus supply.

## Modbus RS485

Rev A uses **ISOW1412DFM**.

Core-side supplies:

```text
VIO     3V3
VDD     SYS_5V / BOARD_5V
GNDIO   CORE_GND
GND1    CORE_GND
```

Isolated-side setup:

```text
VISOOUT -> filtering -> VISOIN
GND2    -> filtering -> GISOIN
MODE    -> VISOOUT   (5-V isolated bus-side configuration)
```

Use the TI-recommended local decoupling around VIO/VDD and VISOOUT/VISOIN. `EN/FLT` is pulled up to 3V3 with 4.7 kOhm.

Half duplex:

```text
GPIO4  -> D
GPIO2  <- R
GPIO5  -> DE and /RE

Y + A -> MODBUS_A
Z + B -> MODBUS_B
```

Protection and termination:

- SM712 directly at the connector
- 120-ohm termination through `SJ_MB_TERM`, default open
- 680-ohm pull-up and pull-down through separate solder jumpers, default open
- test pads for A/B, isolated supply/reference and UART/direction signals

## Victron VE.Bus

VE.Bus uses a second independent **ISOW1412DFM** with the same supply/decoupling strategy.

```text
GPIO48 -> D
GPIO47 <- R
GPIO6  -> DE and /RE
baud   256000
```

RJ45 Rev-A assignment:

| Pin | Signal | Connection |
|---:|---|---|
| 1 | NC | open |
| 2 | V+ | isolated DC/DC input |
| 3 | GND | VE.Bus reference / isolated DC/DC return |
| 4 | A | isolated VE.Bus transceiver A |
| 5 | B | isolated VE.Bus transceiver B |
| 6 | STB | optional isolated footprint, DNI |
| 7 | PD | optional isolated footprint, DNI |
| 8 | NC | open |

VE.Bus termination and bias footprints are present but DNI/open by default until the target network is characterized.

## GPIO allocation

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

GPIO3, GPIO45 and GPIO46 remain unused because they are ESP32-S3 strapping pins. GPIO26 remains reserved for routing/revision margin.

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

## PCB layout rules

- 4-layer PCB preferred.
- Keep the LMR38020 hot loop compact and away from RF/GNSS.
- Maintain isolation clearances with no copper crossing either ISOW1412 barrier or the isolated VE.Bus DC/DC barrier.
- Keep Modbus and VE.Bus isolated domains physically separate.
- Place TVS devices directly at connectors.
- Route RS485 A/B as coupled differential pairs with very short stubs.
- Keep LoRa antenna keep-out completely clear.
- Keep GNSS feed/antenna away from switching-node copper.
- Keep `JP_USB_SAFE` physically accessible with the Heltec installed.

## Required Rev-A test points

```text
VIN_FIELD
VPROTECTED
EXT_5V
SYS_5V
HELTEC_5V
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

## Hardware status

The Rev-A topology and detailed electrical baseline are defined. Remaining measured selections are:

- exact VE.Bus isolated DC/DC module/input range after `VEBUS_VPLUS` measurement
- final source-OR diode verification under worst-case load
- VE.Bus A/B polarity, idle bias and native termination
- STB/PD electrical behaviour
- surge/EMC and thermal performance
- KiCad ERC and final footprint/layout verification

The detailed BOM is in `hardware/rev-a/bom-critical.csv`.
