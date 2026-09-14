# Rev-A Heltec V4.2 FEM correction

## Status

**Mandatory correction before Rev-A PCB release.**

During SX1262/LoRaWAN bring-up the current Heltec WiFi LoRa 32 V4.2 board definitions were cross-checked against the carrier pin allocation. The V4.2 onboard GC1109 RF front-end consumes three ESP32-S3 GPIOs that must not be assigned to carrier peripherals:

| GPIO | Heltec V4.2 onboard function | Carrier availability |
|---:|---|---|
| 7 | GC1109/FEM power | unavailable |
| 2 | GC1109/FEM enable | unavailable |
| 46 | GC1109 PA mode / TX enable | unavailable |

The previous Rev-A allocation used GPIO2 as `MODBUS_RX`. That allocation conflicts with the onboard RF front-end and is invalid for Heltec V4.2.

## Corrected Rev-A allocation

```text
GPIO33 <- MODBUS_RX
GPIO4  -> MODBUS_TX
GPIO5  -> MODBUS_DIR

GPIO7  -> LORA_FEM_POWER   (onboard Heltec resource)
GPIO2  -> LORA_FEM_ENABLE  (onboard Heltec resource)
GPIO46 -> LORA_FEM_PA      (onboard Heltec resource)
```

Physical header mapping:

```text
J2.12 / GPIO33 -> MODBUS_RX
J3.13 / GPIO2  -> reserved onboard GC1109 FEM enable
J3.18 / GPIO7  -> reserved onboard GC1109 FEM power
J3.5  / GPIO46 -> reserved onboard GC1109 PA-mode control
```

## Release rule

No Rev-A schematic, PCB, netlist, BOM package or fabrication output may be released if `MODBUS_RX` is connected to GPIO2. The authoritative machine-checked allocation is `hardware/rev-a/heltec-v4.2-pinmap.csv` together with `include/board/BoardPins.h` and `tests/rev_a_pinmap_test.py`.

The older references in `docs/hardware-rev-a.md` that still show GPIO2 as `MODBUS_RX` are superseded by this correction until that document is regenerated/updated from the corrected pin map.

## RF control used by firmware

The LoRaWAN transport initializes the V4.2 radio path with:

- GC1109 FEM power enabled on GPIO7,
- GC1109 FEM enabled on GPIO2,
- PA mode on GPIO46 low for idle/RX and high for TX,
- SX1262 DIO2 enabled as the internal RF switch control,
- 1.8-V TCXO control through SX1262 DIO3.

RadioLib's RF-switch table changes GPIO46 automatically between TX and RX so RX1/RX2 windows use the correct front-end state.
