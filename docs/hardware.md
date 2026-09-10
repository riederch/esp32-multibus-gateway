# Hardware

## Reference prototype

Reference development board:

- **Heltec HTIT-WB32LAF V4.2 / WiFi LoRa 32 V4.2**
- ESP32-S3
- SX1262 LoRa transceiver for EU868
- onboard OLED display
- onboard user button and LEDs
- native USB-C
- Wi-Fi and Bluetooth LE
- BAT connector / battery measurement support
- SOL connector / solar charging path where supported by the board
- Vext / board-controlled peripheral supply where supported
- external GNSS connector; the exact ordered GNSS module type is still to be confirmed

The first prototype should preserve use of the module as much as possible and add only the external field-interface circuitry required for VE.Bus and Modbus.

## External connectors

### 4-pole field terminal

Target pinout:

```text
V+   external supply input
V-   supply return
A    RS485 / Modbus A
B    RS485 / Modbus B
```

External supply target range: **5-30 V DC**.

This is intentionally similar to the practical UC100 wiring concept.

### Victron-compatible RJ45

The RJ45 connector is optional and belongs to the Victron component.

VE.Bus pinout:

| RJ45 pin | Signal | Use |
|---:|---|---|
| 1 | NC | leave open |
| 2 | V+ | optional gateway power source |
| 3 | GND | VE.Bus reference / power return |
| 4 | A | VE.Bus differential data A |
| 5 | B | VE.Bus differential data B |
| 6 | STB / Standby | optional Victron control |
| 7 | PD / Panel Detect | optional Victron control |
| 8 | NC | leave open |

Do not confuse VE.Bus with VE.Can; the same connector family is used with different pin assignments and protocols.

## Power architecture

The platform supports two normal power sources:

1. external supply on the field terminal (`V+ / V-`), target 5-30 V DC
2. VE.Bus V+ / GND when a Victron device is connected

Neither source is mandatory by itself.

The two sources must **not** be directly paralleled. The final design requires protected source selection / power ORing so one input cannot back-feed the other.

Concept:

```text
External V+ 5-30 V --- protection ---+
                                     +--- power OR / isolation --- regulated board supply
VE.Bus V+ ----------- protection ----+
```

The exact converter topology must be selected after measuring the actual VE.Bus voltage/current capability and confirming the acceptable power input path for the HTIT-WB32LAF V4.2.

### VE.Bus supply validation

For the initial Victron target, MultiPlus 12/500/20-16, validate:

- open-circuit V+ voltage
- voltage under approximately 100 mA input load
- voltage under approximately 250 mA input load
- optionally 500 mA if electrically reasonable
- available continuous power
- behaviour during inverter/charger off, standby and low-power states

VE.Bus V+ must be treated as a device-derived supply, not as an assumed regulated 12.0 V auxiliary rail.

## Electrical isolation

VE.Bus and Modbus are separate electrical domains and require independent transceiver paths.

Target production concept:

```text
VE.Bus A/B <-> isolated RS485 <-> ESP32-S3
Modbus A/B <-> isolated RS485 <-> ESP32-S3
```

Power-domain design must also ensure that connecting USB to a PC cannot accidentally bypass the intended VE.Bus isolation.

Depending on the final power topology this may require an isolated DC/DC domain or equivalent isolation strategy on the VE.Bus side.

## VE.Bus RS485 channel

Requirements:

- 256000 baud capability
- isolated differential interface
- explicit DE/RE control preferred for final hardware
- verified VE.Bus turnaround timing
- termination/biasing matched to the actual topology

Automatic-direction RS485 modules may be used only for early experiments if timing proves adequate; they are not assumed production-suitable.

## Modbus RS485 channel

Requirements:

- independent isolated RS485 interface
- configurable baud rate
- configurable parity
- configurable stop bits
- Modbus RTU master or slave depending on configuration
- raw/transparent access capability
- optional selectable 120 ohm termination

## Native board resources

The firmware should expose usable onboard resources through the Platform abstraction instead of binding them to one application.

### OLED display

Uses:

- system status
- Wi-Fi/AP credentials during commissioning
- IP / mDNS hostname
- component status
- alarms and diagnostics
- Victron values when enabled
- Modbus state
- LoRaWAN state
- GNSS state
- power/battery state

### User button

Expected uses:

- short press: cycle display pages / context action
- deliberate long press: complete factory reset with visible warning/countdown
- commissioning/AP entry may be assigned as a separate safe interaction during implementation

There is no physical button function that resets only the administrator password.

### BAT and SOL

Board battery/solar facilities should be represented by the Platform Power service where electrically supported by the exact board revision.

Desired exposed properties/capabilities include:

- battery present
- battery voltage
- charging state where detectable
- external/solar source state where detectable

Exact behaviour and safe simultaneous use of USB, external V+, BAT, SOL and VE.Bus-derived power must be validated against the V4.2 schematic before final wiring.

### Vext and expansion I/O

Where supported by the board, expose switchable Vext and unused expansion resources through a generic I/O abstraction:

- digital GPIO
- ADC
- PWM
- touch
- I2C
- SPI
- spare UART resources

Exact free GPIOs are not yet fixed. Pin assignment must be done only after checking conflicts with SX1262, OLED, GNSS, native USB, user controls and power-management signals.

## GNSS

An external GNSS module has been ordered for the HTIT-WB32LAF V4.2 platform.

The GNSS component should support, where provided by the actual module/interface:

- UART receive/transmit
- latitude/longitude
- altitude
- UTC time
- satellite/fix quality
- speed/course
- PPS
- power control / wake / reset where available

The exact module type and pin-level capabilities remain a validation item until the ordered hardware is identified.

## USB-C

Native ESP32-S3 USB is retained for:

- initial flashing
- recovery
- diagnostics/service
- firmware updates where suitable
- Victron MK2/MK3 USB compatibility experiments when `V=1`

USB D+/D- pins must remain reserved for native USB operation.

## Bluetooth LE

BLE is available as a platform radio. Generic platform administration should primarily use Wi-Fi/WebUI. When Victron is enabled, BLE may be claimed by the Victron cocoon for VictronConnect/Smart-Dongle compatibility experiments.

## LoRa

The onboard SX1262 is owned by the LoRa component.

Supported architecture modes:

- LoRaWAN (`L=W`)
- Meshtastic (`L=M`, backlog)
- disabled (`L=0`)

Only one radio stack may own the SX1262 at a time.

## Initial Victron target

- Victron MultiPlus 12/500/20-16

Direct VE.Bus control and compatibility emulation remain experimental until validated on controlled hardware.
