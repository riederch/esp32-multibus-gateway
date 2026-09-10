# LoRaWAN protocol

## Profiles

The LoRaWAN layer exposes two protocol profiles:

1. **Compatibility profile** on FPort 85
2. **MultiBus extension profile** on a separate configurable FPort

## Compatibility profile

FPort 85 is reserved for the Milesight UC100 V2-compatible wire protocol.

The compatibility goal is strict:

- existing UC100 V2 uplink decoders must accept MultiBus compatibility payloads
- existing UC100 V2 downlink encoders must remain valid for the standard command set
- existing command identifiers, field meanings, byte order and framing must not be redefined
- unsupported/reserved identifiers remain reserved
- Milesight D2D radio operation is not implemented

The UC100 is used here as a protocol-compatibility reference; the internal MultiBus architecture is independent of it.

## Standard command coverage

The compatibility implementation must cover the complete applicable UC100 V2 LoRaWAN command set, including configuration and control for:

- reporting intervals and reporting behaviour
- Modbus/RS485 serial settings
- Modbus channels
- channel reads/reports
- threshold/change alarms and alarm releases
- retransmission/store-and-forward controls
- history retrieval/control
- time/timezone/DST related settings
- rule/IF-THEN related controls where represented by the protocol
- raw/transparent RS485 operations
- device reboot/rejoin and other standard management commands

Compatibility work is considered complete only when the protocol table has been checked command-by-command against the current UC100 V2 protocol documentation and covered by encoder/decoder tests.

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

This lets Victron-derived data use the same telemetry, alarm and history framing as Modbus-derived values.

## Victron configuration

The original compatibility command set cannot describe a VE.Bus property binding. Therefore source binding is separated from channel behaviour.

A Victron channel is created/configured through the MultiBus extension profile or Web UI:

```text
channel 20
source = victron:vebus
point  = battery.voltage
```

After binding, channel 20 participates in the normal compatibility mechanisms:

- reporting
- alarms
- history
- retransmission
- remote enable/disable or interval controls where the standard command applies to the logical channel

No fake Modbus slave ID or register address is required internally.

## MultiBus extension profile

Features that cannot be represented without changing FPort-85 semantics use a separate extension FPort.

Extension examples:

- bind a channel to a Victron property
- bind a channel to GNSS/platform I/O
- Victron-specific commands/settings
- capability discovery
- source discovery
- platform-specific diagnostics
- future optional functions

Extension messages must be independently versioned.

## Compatibility invariants

- FPort 85 is never used for incompatible MultiBus-specific payloads.
- Existing FPort-85 command IDs are never repurposed.
- Extension commands never rely on a standard decoder silently interpreting new semantics.
- A compatibility channel has stable logical channel identity regardless of its native source.
- Transport-specific source details remain inside source adapters.

## Testing

Maintain protocol-vector tests containing:

- known valid UC100 V2 downlinks and expected parsed commands
- expected binary uplinks for configured standard channels
- alarm/release vectors
- history/retransmission vectors
- raw RS485 command vectors
- round-trip tests for all supported configuration commands
- regression tests proving extension traffic never appears on FPort 85

Where public vendor examples exist, preserve those binary vectors as interoperability fixtures without copying copyrighted explanatory text.
