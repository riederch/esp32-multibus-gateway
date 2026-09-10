# LoRaWAN protocol

## Role

LoRaWAN is a transport between the Core and the LoRaWAN network. It does not own Modbus, Victron, GNSS or platform values directly. Outbound data is encoded from normalized channels/events/history, and inbound downlinks are decoded into validated Core commands/events.

```text
DataSource -> point -> channel/event/history -> LoRaWAN encoder -> SX1262
SX1262 -> LoRaWAN decoder -> validated command/event -> Core -> target source/service
```

## Activation and identity

OTAA is the primary activation method.

### DevEUI

The DevEUI is the stable 64-bit identity of one physical gateway device.

Requirements:

- unique per device
- stable across normal reboots and configuration changes
- not regenerated silently during factory reset when it represents provisioned hardware identity
- visible to an authenticated administrator for provisioning
- production devices must support provisioning a formally assigned EUI-64

For development/private deployments, a deterministic identifier derived from the ESP32 factory identity may be used as the default. The derivation must preserve per-device uniqueness and be documented in the implementation.

### JoinEUI

The JoinEUI is a configured 64-bit identifier for the intended join/application domain.

Requirements:

- may be shared by multiple MultiBus devices
- is not generated randomly per device
- configurable through authorized provisioning
- stored as LoRaWAN configuration

`JoinEUI` is the current LoRaWAN name for what older tooling may call `AppEUI`.

### AppKey

The AppKey is the per-device 128-bit OTAA root key.

Requirements:

- cryptographically random or securely provisioned
- unique per device
- stored as a secret
- never written to unauthenticated logs or diagnostics
- may be revealed/copyable only through an explicit authenticated provisioning action
- included in portable backup only when the backup is protected appropriately

A factory reset removes mutable LoRaWAN credentials. A new AppKey may then be generated during reprovisioning unless a key is explicitly imported.

### ChirpStack Application ID

The ChirpStack Application ID is server-side metadata and is not a LoRaWAN air-interface identifier. The end device does not need the ChirpStack Application UUID for OTAA.

A future provisioning integration may store or use a ChirpStack target/application reference externally, but it must remain separate from DevEUI, JoinEUI and AppKey.

## Profiles

The LoRaWAN layer exposes two protocol profiles:

1. **Compatibility profile** on FPort 85
2. **MultiBus extension profile** on a separate configurable FPort

## Compatibility profile

FPort 85 is reserved for the Milesight UC100 V2-compatible wire protocol.

The compatibility target is strict for the complete applicable non-D2D UC100 V2 LoRaWAN command set:

- existing compatible uplink decoders must accept MultiBus compatibility payloads
- existing compatible downlink encoders must remain valid
- existing command identifiers, field meanings, byte order and framing must not be redefined
- command behaviour must match the reference semantics wherever the corresponding feature exists
- unsupported/reserved identifiers remain reserved
- proprietary Milesight D2D operation is the deliberate compatibility exception

The reference protocol influences the compatibility codec only; the internal MultiBus architecture remains source- and transport-neutral.

## Command coverage

The compatibility implementation must cover the applicable command set, including:

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
- other non-D2D management/configuration commands defined by the reference protocol

Compatibility is complete only after the protocol table has been verified command-by-command against the reference documentation and covered by encoder/decoder tests.

## Generic channel bridge

LoRaWAN consumes normalized channels rather than native Modbus registers or VE.Bus properties.

```text
native source -> normalized point -> channel -> compatibility encoder -> FPort 85
```

Examples:

```text
modbus:12/pressure.bar
    -> channel 1
    -> compatibility channel uplink

victron:vebus/battery.voltage
    -> channel 20
    -> compatibility channel uplink

gnss:primary/speed
    -> channel 30
    -> compatibility channel uplink where datatype/semantics fit
```

This lets different native sources use the same telemetry, alarm, history and retransmission framing.

## Non-Modbus channel bindings

The compatibility protocol can configure Modbus channels because its source model contains Modbus slave/register information. It has no native representation for a VE.Bus, GNSS or platform property.

MultiBus therefore separates **source binding** from **channel behaviour**.

Example:

```text
channel 20
source = victron:vebus
point  = battery.voltage
```

After binding, channel 20 behaves like a normal compatibility channel for operations that apply to the logical channel:

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
- additional MultiBus functions

Extension messages are independently versioned.

## Compatibility invariants

- FPort 85 never carries incompatible MultiBus-specific payloads.
- Existing FPort-85 command IDs are never repurposed.
- Extension commands never depend on a compatibility decoder silently interpreting new semantics.
- A compatibility channel keeps a stable logical channel identity regardless of its native source.
- Native source details remain inside source adapters.
- LoRaWAN remains a transport and never becomes a parent/owner of native protocol components.

## Testing

Maintain protocol-vector tests containing:

- known valid reference downlinks and expected parsed commands
- expected binary uplinks for configured standard channels
- alarm/release vectors
- history/retransmission vectors
- raw RS485 command vectors
- round-trip tests for every supported configuration command
- regression tests proving extension traffic never appears on FPort 85
- provisioning tests for DevEUI/JoinEUI/AppKey persistence and reset behaviour

Public vendor binary examples may be retained as interoperability test vectors without reproducing copyrighted explanatory material.
