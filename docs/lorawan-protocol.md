# LoRaWAN protocol

## Profiles

The LoRaWAN layer exposes two protocol profiles:

1. **Compatibility profile** on FPort 85
2. **MultiBus extension profile** on a separate configurable FPort

## Compatibility profile

FPort 85 is reserved for the Milesight UC100 V2-compatible wire protocol.

The compatibility target is strict for the complete non-D2D UC100 V2 LoRaWAN command set:

- existing UC100 V2 uplink decoders must accept MultiBus compatibility payloads
- existing UC100 V2 downlink encoders must remain valid
- existing command identifiers, field meanings, byte order and framing must not be redefined
- command behaviour must match the reference semantics wherever the corresponding feature exists
- unsupported/reserved identifiers remain reserved
- proprietary Milesight D2D operation is the only deliberate compatibility exception

The UC100 is used only as a protocol-compatibility and feature-design reference; the internal MultiBus architecture is independent of it.

## Command coverage

The compatibility implementation must cover the full applicable command set, including:

- reporting interval and periodic reporting controls
- reboot and LoRaWAN rejoin
- data storage
- retransmission enable/disable and interval
- UTC timezone and daylight-saving configuration
- LNS time synchronization
- Modbus/RS485 serial settings
- Modbus channel create/configure/delete/name operations
- channel acquisition/reporting
- threshold alarms, alarm release and change alarms
- local rule/IF-THEN configuration where represented by the protocol
- raw/transparent RS485 operations
- historical-data retrieval and transfer controls
- all other non-D2D management/configuration commands defined by the reference protocol

Compatibility is complete only after the protocol table has been verified command-by-command against the current UC100 V2 protocol documentation and covered by encoder/decoder tests.

## Generic channel bridge

Internally, LoRaWAN does not read Modbus registers directly. It consumes normalized channels.

```text
native source -> normalized point -> channel -> compatibility encoder -> FPort 85
```

Examples:

```text
modbus:12/pressure.bar
    -> channel 1
    -> standard compatibility channel uplink

victron:vebus/battery.voltage
    -> channel 20
    -> standard compatibility channel uplink
```

This lets Victron-derived data use the same telemetry, alarm, history and retransmission framing as Modbus-derived values.

## Victron channel bindings

The compatibility protocol can configure Modbus channels because its source model contains Modbus slave/register information. It has no native representation for a VE.Bus property.

MultiBus therefore separates **source binding** from **channel behaviour**.

A Victron-backed channel is bound through the MultiBus extension profile or Web UI:

```text
channel 20
source = victron:vebus
point  = battery.voltage
```

After binding, channel 20 behaves like a normal compatibility channel for all operations that apply to a logical channel:

- periodic reporting
- alarms and alarm releases
- history
- retransmission
- enable/disable and reporting controls where defined by the compatibility protocol

No fake Modbus slave ID or register address is created internally.

## MultiBus extension profile

Features that cannot be represented without changing FPort-85 semantics use a separate extension FPort.

Extension functions include:

- bind a channel to a Victron property
- bind a channel to GNSS/platform I/O
- Victron-specific settings/commands
- capability discovery
- source/point discovery
- platform diagnostics
- additional features introduced by MultiBus

Extension messages are independently versioned.

## Compatibility invariants

- FPort 85 never carries incompatible MultiBus-specific payloads.
- Existing FPort-85 command IDs are never repurposed.
- Extension commands never depend on a standard decoder silently interpreting new semantics.
- A compatibility channel keeps a stable logical channel identity regardless of its native source.
- Transport-specific source details remain inside source adapters.

## Testing

Maintain protocol-vector tests containing:

- known valid UC100 V2 downlinks and expected parsed commands
- expected binary uplinks for configured standard channels
- alarm/release vectors
- history/retransmission vectors
- raw RS485 command vectors
- round-trip tests for every supported configuration command
- regression tests proving extension traffic never appears on FPort 85

Public vendor binary examples may be retained as interoperability test vectors without reproducing copyrighted explanatory material.
