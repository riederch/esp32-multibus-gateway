# Hardware notes

## Target installation concept

Normal installed operation should require only two field connections:

1. **VE.Bus RJ45** to the Victron device - communication **and gateway power**
2. **Modbus RS485 terminal** - A / B / optional isolated reference

Additional service interfaces are wireless Bluetooth LE and a USB-C service port. USB is not required during normal installed operation.

## Prototype

Recommended first prototype:

- ESP32-S3 DevKitC-1 or compatible board with native USB
- one galvanically isolated RS485 channel for VE.Bus
- one separate galvanically isolated RS485 channel for Modbus RTU
- one RJ45 breakout for VE.Bus
- one terminal block for Modbus RS485
- one DC/DC converter supplied **only from VE.Bus V+ / GND**

## VE.Bus channel

Requirements:

- isolated RS485 transceiver path
- 256000 baud capability
- explicit TX driver-enable control preferred
- termination/biasing must match the actual VE.Bus topology

A generic automatic-direction RS485 module may be useful for initial experiments, but should not be assumed suitable for final VE.Bus hardware until turnaround timing has been measured.

### VE.Bus RJ45 pinout

| RJ45 pin | Signal | T568B wire colour | Gateway use |
|---:|---|---|---|
| 1 | NC | white/orange | leave open |
| 2 | V+ | orange | **gateway power input** |
| 3 | GND | white/green | VE.Bus-side supply return/reference |
| 4 | A | blue | VE.Bus differential data A |
| 5 | B | white/blue | VE.Bus differential data B |
| 6 | STB / Standby | green | optional later control |
| 7 | PD / Panel Detect | white/brown | optional later control |
| 8 | NC | brown | leave open |

Pins 6 and 7 remain unconnected in the first prototype until the corresponding control behaviour is explicitly implemented and tested.

Do not confuse VE.Bus with VE.Can. They use the same RJ45 connector family but different electrical pin assignments and protocols.

## Power architecture - VE.Bus only

The target gateway has **no separate DC power connector**. It is powered directly from VE.Bus:

```text
VE.Bus RJ45
  pin 2 V+ ---- protection ---- isolated DC/DC ---- 5 V SYS ---- ESP32-S3
  pin 3 GND --- protection ---- isolated DC/DC input return
  pin 4 A ---------------------- VE.Bus RS485 bus side
  pin 5 B ---------------------- VE.Bus RS485 bus side
```

The DC/DC input voltage range must cover the actual VE.Bus V+ voltage of the target device. For the initial MultiPlus 12/500/20-16 prototype, a roughly **9-30 V input to regulated 5 V** converter is the provisional choice, but the voltage and available current on pin 2 must be measured before the final component is selected.

Required validation on the target MultiPlus:

- open-circuit voltage pin 2 to pin 3
- voltage under approximately 100 mA input load
- voltage under approximately 250 mA input load
- optionally voltage under approximately 500 mA input load
- behaviour when the MultiPlus changes operating state or enters low-power/off states

Do not document VE.Bus V+ as a guaranteed fixed 12.0 V auxiliary supply unless confirmed by device-specific documentation or measurement.

### Isolation and USB safety

Because the gateway can be connected to a PC/phone through USB while simultaneously connected to VE.Bus, the ESP32/USB ground must not accidentally bypass the intended VE.Bus galvanic isolation.

Preferred final topology:

- VE.Bus pin 2/3 feed the **input side of an isolated DC/DC converter**
- ESP32-S3 and USB are powered on the isolated system side
- VE.Bus A/B use an isolated RS485 transceiver back to the VE.Bus side
- Modbus uses a second independent isolated RS485 channel

This keeps VE.Bus, system/USB and Modbus domains galvanically separated.

### Power budget

The 5 V system rail should provide at least **1 A**, preferably up to **2 A** of short-term capability for ESP32-S3 Wi-Fi/BLE peaks and interface circuitry. The actual VE.Bus V+ current budget must be measured; converter output rating alone does not prove that VE.Bus can supply that power continuously.

## Modbus channel

Requirements:

- separate isolated RS485 transceiver
- configurable baud rate, parity and stop bits
- A/B terminal connection, with isolated reference terminal if useful
- optional 120 ohm termination selectable by jumper or switch

The Modbus and VE.Bus interfaces must remain electrically and logically independent and must not share one switched RS485 transceiver.

## Bluetooth LE

The ESP32-S3 BLE interface is intended for:

- gateway commissioning
- diagnostics
- optional future VictronConnect / VE.Bus Smart Dongle compatibility experiments

BLE requires no additional external connector.

## USB-C service interface

Use the ESP32-S3 native USB-OTG peripheral as the default USB service interface.

Target functions:

1. firmware flashing / diagnostics where practical
2. serial debug/service access
3. experimental **MK2/MK3-compatible protocol transport**
4. investigate direct VictronConnect compatibility

The original Victron MK3-USB presents a USB serial interface based on an FTDI FT232EX. Therefore native ESP32 USB compatibility with VictronConnect must be treated as an experimental layer, not assumed automatically.

The firmware should separate the **MK2/MK3 protocol engine** from the USB transport so that USB backends can be changed later without touching the VE.Bus protocol implementation.

For a self-powered ESP32-S3 USB device, include USB VBUS presence sensing as required by the ESP32-S3 USB device design rules.

## Target device

Initial VE.Bus target:

- Victron MultiPlus 12/500/20-16

No assumption should be made that all VE.Bus device generations behave identically.
