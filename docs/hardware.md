# Hardware

## Reference platform

Reference board:

- Heltec HTIT-WB32LAF V4.2 / WiFi LoRa 32 V4.2
- ESP32-S3
- SX1262 LoRa transceiver for EU868
- onboard OLED
- onboard user button and LED
- native USB-C
- Wi-Fi and Bluetooth LE
- BAT connector and battery measurement support
- SOL connector / solar charging path
- switchable Vext
- external GNSS connector

External circuitry adds two independent isolated RS485 paths: one for Modbus and one for optional Victron VE.Bus.

## Field terminal

```text
V+   external supply input, target 5-30 V DC
V-   supply return
A    Modbus RS485 A
B    Modbus RS485 B
```

## Victron RJ45

| RJ45 pin | Signal | Use |
|---:|---|---|
| 1 | NC | open |
| 2 | V+ | optional gateway power source |
| 3 | GND | VE.Bus reference / power return |
| 4 | A | VE.Bus differential data A |
| 5 | B | VE.Bus differential data B |
| 6 | STB / Standby | optional Victron control |
| 7 | PD / Panel Detect | optional Victron control |
| 8 | NC | open |

VE.Bus and VE.Can use similar connectors but different pin assignments and protocols.

## Power architecture

Supported source concepts:

1. external 5-30 V supply on `V+ / V-`
2. optional VE.Bus V+ / GND supply
3. board BAT / SOL facilities within the limits of the final power topology
4. USB-C during service/programming

Independent sources must not be directly paralleled or allowed to back-feed one another.

```text
External V+ ---- protection ----+
                                +---- protected source selection ---- system supply
VE.Bus V+ ------ protection ----+

BAT / SOL / USB ---- board power paths subject to board schematic constraints
```

The final design must preserve safe isolation when USB is connected while field buses are attached.

## Electrical isolation

VE.Bus and Modbus are separate electrical domains.

```text
VE.Bus A/B <-> isolated RS485 <-> ESP32-S3
Modbus A/B <-> isolated RS485 <-> ESP32-S3
```

The two buses never share one switched transceiver.

## VE.Bus interface

Requirements:

- 256000 baud capability
- isolated differential interface
- explicit DE/RE control for production hardware unless measured timing proves an alternative safe
- validated A/B polarity
- validated bias/termination behaviour
- validated turnaround timing

Initial compatibility target: Victron MultiPlus 12/500/20-16.

## Modbus interface

Requirements:

- independent isolated RS485 path
- configurable baud rate
- configurable parity
- configurable stop bits
- Modbus RTU master or slave
- raw/transparent access
- selectable 120-ohm termination where practical
- defined fail-safe/biasing strategy

## Verified onboard pin map

HTIT-WB32LAF V4.2 onboard resources used by the firmware abstraction:

```text
GPIO0   USER / PRG button
GPIO1   battery ADC input
GPIO35  user LED
GPIO36  Vext control
GPIO37  battery ADC control

GPIO17  OLED SDA
GPIO18  OLED SCL
GPIO21  OLED reset

GPIO34  GNSS power control
GPIO38  GNSS RX
GPIO39  GNSS TX
GPIO40  GNSS wake
GPIO41  GNSS PPS
GPIO42  GNSS reset

GPIO8..14  SX1262-related signals according to the board pin map
```

VE.Bus and Modbus UART/DE/RE pins are assigned only from pins remaining free after all onboard functions are accounted for.

## OLED

The OLED is the local status and commissioning interface.

Display content includes:

- device name
- Wi-Fi state / IP / hostname
- AP SSID and password during commissioning
- one-time administrator password during initial setup
- LoRaWAN state
- Modbus state
- Victron state
- GNSS state
- battery/power state
- alarms and diagnostics
- factory-reset countdown

## User button

- short press: display navigation/context action
- deliberate long press: complete factory reset
- no password-only reset path

## BAT / SOL / Vext

Platform power services expose, where electrically detectable:

- battery voltage
- battery-present state
- charging state
- solar/external charging state
- Vext enable/disable

Safe simultaneous use of USB, external V+, BAT, SOL and VE.Bus-derived power is a hardware validation requirement.

## GNSS

The GNSS interface provides:

- UART RX/TX
- power control
- wake
- reset
- PPS

The GNSS component normalizes position/time/movement information for the Core.

## USB-C

Native ESP32-S3 USB is used for:

- flashing
- service / diagnostics
- recovery
- firmware update
- optional MK2/MK3 compatibility transport when Victron is enabled

## Bluetooth LE

BLE is a platform radio. Generic administration uses Wi-Fi/WebUI. Victron may additionally use BLE for VictronConnect compatibility.

## LoRa

The onboard SX1262 is owned by exactly one LoRa backend at a time:

- LoRaWAN (`L=W`)
- Meshtastic (`L=M`)
- disabled (`L=0`)
