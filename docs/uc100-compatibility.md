# UC100 V2 compatibility matrix

This document tracks the implemented Milesight UC100 V2-compatible LoRaWAN
surface on FPort 85.

Status terminology:

- **Implemented**: codec/runtime support exists and is covered by host tests
  and the normal firmware CI build.
- **Reserved**: intentionally not implemented because it belongs to proprietary
  Milesight D2D operation.
- **Validation pending**: implementation exists, but interoperability still
  requires end-to-end validation with representative hardware/network-server
  setups.

## Basic information uplinks

| Item | Wire ID | Status | Notes |
|---|---|---|---|
| Power on | `FF 0B` | Implemented | Sent after LoRaWAN join |
| Protocol version | `FF 01` | Implemented | Compatibility protocol identifier |
| TSL version | `FF FF` | Implemented | Compatibility metadata |
| Device serial number | `FF 16` | Implemented | Stable value derived from hardware identity |
| Hardware version | `FF 09` | Implemented | Reference Heltec V4.2 hardware |
| Software version | `FF 0A` | Implemented | MultiBus development value until release versioning is introduced |
| Device type / class | `FF 0F` | Implemented | Class A or Class C from configuration |
| Reset event | `FF FE FF` | Implemented | Appended once after reset |

## Basic settings

| Item | Wire ID | Status |
|---|---|---|
| Reporting interval | `FF 03` | Implemented |
| Rejoin network | `FF 04 FF` | Implemented |
| Reboot | `FF 10 FF` | Implemented |
| Enquiry periodic report | `FF 28 FF` | Implemented |
| Data storage | `FF 68` | Implemented |
| Data retransmission | `FF 69` | Implemented |
| Retransmission interval | `F9 0D` | Implemented |
| UTC timezone | `FF BD` | Implemented |
| Sync time with LNS | `FF 4A 00` | Implemented |
| Daylight saving time | `F9 72` | Implemented |

## RS485 / Modbus

| Item | Wire ID | Status | Notes |
|---|---|---|---|
| RS485 serial settings | `F9 78` | Implemented | Runtime rejects serial combinations unsupported by ESP32 UART |
| Modbus settings | `F9 79` | Implemented | Poll timing, retries and pass-through |
| Enquiry RS485 settings | `F9 7A` | Implemented | Serial and Modbus settings replies |
| Modbus channel create/update/delete/name | `FF EF` | Implemented | Up to 32 compatibility slots |
| Periodic channel values | `F9 73` | Implemented | Fragmented across FPort-85 packets as required |
| Collection exception | `FF 15` | Implemented | Poll failure reporting |
| Modbus writes | RTU FC05/06/16 | Implemented | Non-blocking runtime write path |
| Active pass-through | `F9 79 mode 10` | Implemented | Server-driven RS485 bridge |
| Two-way pass-through | `F9 79 mode 11` | Implemented | Unsolicited RS485 frames forwarded to configured LoRaWAN FPort |
| Passive RS485 receive | — | Implemented | Shared primitive for rules and Two-way pass-through |

When Two-way pass-through is enabled, normal Modbus channel polling and
DataSource reads/writes are suspended, matching the reference behavior that
Modbus channels are unavailable in this mode.

## History and store-and-forward

| Item | Wire ID | Status |
|---|---|---|
| Historical Modbus record | `21 CE` | Implemented |
| Enquire time point | `FD 6B` | Implemented |
| Enquire time range | `FD 6C` | Implemented |
| Stop historical query | `FD 6D FF` | Implemented |
| Retrievability interval | `F9 0E` | Implemented |
| Enquiry result | `FC 6B/6C` | Implemented |
| Lost-data retransmission | `21 CE` | Implemented |

History is stored in a bounded persistent ring with CRC-protected A/B journal
images. Retransmission starts from the persisted network-loss time marker and
historical queries snapshot their selected records before upload.

## Alarms

| Item | Wire ID | Status |
|---|---|---|
| Threshold alarm | `F9 73` alarm bits | Implemented |
| Threshold release | `F9 73` release bits | Implemented |
| Change alarm | `F9 73` + `F9 74` | Implemented |

Alarm actions preserve the value that caused the trigger/release rather than
sampling a later cached value.

## IF-THEN rules

### Management

| Item | Wire ID | Status |
|---|---|---|
| Rule enable/disable/delete | `F9 76` | Implemented |
| Rule enquiry | `F9 77` | Implemented |
| Rule configuration / acknowledgement | `F9/F8 7D` | Implemented |

Up to 16 compatibility rules are stored persistently. Original `F9 7D`
frames are retained so rule enquiries can reproduce compatible wire data.

### Conditions

| Condition | Subtype | Status |
|---|---:|---|
| Time schedule | `11` | Implemented |
| Modbus channel threshold/change | `12` | Implemented |
| Received command via RS485 | `13` | Implemented |
| Received server message | `14` | Implemented |
| Received D2D control command | `15` | Reserved |
| Device restart | `16` | Implemented |

### Actions

| Action | Suffix | Status |
|---|---:|---|
| No action | `x0` | Implemented |
| Send server message | `x1` | Implemented |
| Send D2D control command | `x2` | Reserved |
| Send command via RS485 | `x3` | Implemented |
| Upload data | `x4` | Implemented |
| Upload alarm packet | `x5` | Implemented |
| Device restart | `x6` | Implemented |

D2D condition/action identifiers are kept reserved and are never repurposed for
MultiBus-specific behavior.

## Deliberate compatibility exception

Milesight D2D is proprietary radio behavior and is not implemented. MultiBus
uses its separate extension FPort and, eventually, the Meshtastic backend for
non-UC100 features. No D2D identifier is reused.

## Validation status

The applicable Non-D2D command families above are implemented in firmware and
covered by automated codec/runtime tests where practical. This does **not** by
itself prove complete field interoperability.

The following remain validation tasks:

- real UC100-compatible decoder/encoder interoperability against ChirpStack
- Class A and Class C end-to-end timing
- representative RS485 devices at supported baud/parity combinations
- Active and Two-way pass-through with real unsolicited RS485 traffic
- rule timing, threshold, alarm and delayed-action behavior under long-running load
- history power-loss recovery and worst-case flash endurance
- maximum LoRaWAN payload behavior across EU868 data rates

See `docs/validation.md` for the broader hardware and protocol validation list.
