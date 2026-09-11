#!/usr/bin/env python3
"""Cross-check the Rev-A Heltec carrier pin map against KiCad and firmware files."""

from __future__ import annotations

import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PINMAP = ROOT / "hardware/rev-a/heltec-v4.2-pinmap.csv"
SYMBOL_LIB = ROOT / "hardware/rev-a/multibus-rev-a.lib"
FOOTPRINT = ROOT / "hardware/rev-a/multibus-rev-a.pretty/Heltec_WiFi_LoRa_32_V4_2.kicad_mod"
BOARD_PINS = ROOT / "include/board/BoardPins.h"

NON_FIRMWARE_USES = {"", "CORE_GND", "HELTEC_5V", "3V3", "STRAPPING_UNUSED"}
STRAPPING_GPIOS = {3, 45, 46}
FIELD_BUS_CONSTANTS = {
    "MODBUS_RX",
    "MODBUS_TX",
    "MODBUS_DIR",
    "VEBUS_RX",
    "VEBUS_TX",
    "VEBUS_DIR",
    "VEBUS_STB_CTRL",
    "VEBUS_PD_CTRL",
}


def die(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def read_pinmap() -> list[dict[str, str]]:
    with PINMAP.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    if len(rows) != 36:
        die(f"pin map must contain 36 rows, found {len(rows)}")

    pads = [int(row["pad"]) for row in rows]
    if sorted(pads) != list(range(1, 37)):
        die(f"pin map pads must be exactly 1..36, found {sorted(pads)}")
    if len(set(row["header_pin"] for row in rows)) != 36:
        die("pin map contains duplicate header pins")

    return rows


def read_physical_symbol() -> dict[int, str]:
    text = SYMBOL_LIB.read_text(encoding="utf-8")
    match = re.search(
        r"DEF HELTEC_V4_2_PHYSICAL\b.*?\nDRAW\n(.*?)\nENDDRAW\nENDDEF",
        text,
        flags=re.DOTALL,
    )
    if not match:
        die("HELTEC_V4_2_PHYSICAL symbol not found")

    pins: dict[int, str] = {}
    for line in match.group(1).splitlines():
        if not line.startswith("X "):
            continue
        parts = line.split()
        if len(parts) < 3:
            die(f"malformed symbol pin line: {line}")
        name = parts[1]
        pin = int(parts[2])
        if pin in pins:
            die(f"duplicate symbol pin number {pin}")
        pins[pin] = name

    if sorted(pins) != list(range(1, 37)):
        die(f"physical symbol must expose pins 1..36, found {sorted(pins)}")
    return pins


def validate_symbol(rows: list[dict[str, str]], symbol: dict[int, str]) -> None:
    for row in rows:
        pad = int(row["pad"])
        header = row["header_pin"].replace(".", "_")
        signal = row["signal"]
        expected_prefix = f"{header}_{signal}"
        actual = symbol[pad]
        if actual != expected_prefix and not actual.startswith(expected_prefix + "_"):
            die(
                f"symbol pin {pad}: expected name starting with {expected_prefix!r}, "
                f"found {actual!r}"
            )

    j2_8 = next(row for row in rows if row["header_pin"] == "J2.8")
    if j2_8["signal"] != "GPIO0":
        die(f"J2.8 must be GPIO0/PRG on Heltec V4.2, found {j2_8['signal']}")
    if "GPIO10" in symbol[8]:
        die("J2.8 regressed to GPIO10; GPIO10 is the onboard LoRa MOSI signal")


def validate_footprint() -> None:
    text = FOOTPRINT.read_text(encoding="utf-8")
    pads = [int(value) for value in re.findall(r'\(pad\s+"(\d+)"\s+', text)]
    if len(pads) != 36:
        die(f"Heltec footprint must contain 36 numbered pads, found {len(pads)}")
    if sorted(pads) != list(range(1, 37)):
        die(f"Heltec footprint pads must be exactly 1..36, found {sorted(pads)}")
    if len(set(pads)) != 36:
        die("Heltec footprint contains duplicate pad numbers")


def read_board_constants() -> dict[str, int]:
    text = BOARD_PINS.read_text(encoding="utf-8")
    return {
        name: int(value)
        for name, value in re.findall(r"constexpr\s+int\s+(\w+)\s*=\s*(\d+)\s*;", text)
    }


def validate_firmware(rows: list[dict[str, str]], constants: dict[str, int]) -> int:
    checked = 0
    for row in rows:
        use = row["rev_a_use"]
        signal = row["signal"]

        if use == "STRAPPING_UNUSED":
            match = re.fullmatch(r"GPIO(\d+)", signal)
            if not match or int(match.group(1)) not in STRAPPING_GPIOS:
                die(f"unexpected strapping-pin declaration: {row}")
            continue

        if use in NON_FIRMWARE_USES:
            continue

        match = re.fullmatch(r"GPIO(\d+)", signal)
        if not match:
            die(f"firmware-bound use {use} has non-GPIO signal {signal}")
        expected_gpio = int(match.group(1))

        if use not in constants:
            die(f"BoardPins.h is missing constant {use}")
        if constants[use] != expected_gpio:
            die(
                f"BoardPins.h {use}={constants[use]} but physical pin map requires "
                f"GPIO{expected_gpio}"
            )
        checked += 1

    for name in FIELD_BUS_CONSTANTS:
        if name not in constants:
            die(f"BoardPins.h is missing field-bus constant {name}")
        if constants[name] in STRAPPING_GPIOS:
            die(f"field-bus constant {name} illegally uses strapping GPIO{constants[name]}")

    if constants.get("USER_BUTTON") != 0:
        die("USER_BUTTON must remain GPIO0")
    if constants.get("LORA_MOSI") != 10:
        die("LORA_MOSI must remain GPIO10")
    if constants["USER_BUTTON"] == constants["LORA_MOSI"]:
        die("USER_BUTTON and LORA_MOSI must not share a GPIO")

    return checked


def main() -> None:
    rows = read_pinmap()
    symbol = read_physical_symbol()
    validate_symbol(rows, symbol)
    validate_footprint()
    constants = read_board_constants()
    firmware_bindings = validate_firmware(rows, constants)

    print(
        "Rev-A Heltec pin map OK: "
        f"36 header pins, 36 footprint pads, {firmware_bindings} firmware bindings checked."
    )


if __name__ == "__main__":
    main()
