# MQTT

MultiBus can publish normalized channel state and accept commands for writable
channels over MQTT when Wi-Fi client connectivity is available.

## Configuration

MQTT is disabled by default. The persisted configuration contains:

- broker host and TCP port
- optional username/password
- topic prefix (default `multibus`)
- publish interval in seconds
- retained-state toggle

The MQTT device identifier is derived from the ESP32 hardware identity via the
default MultiBus hostname. It does not change when the user edits the Wi-Fi
hostname.

MQTT can use plain TCP or TLS. When TLS is enabled, a CA certificate in PEM
format is required and certificate verification remains enabled. There is no
insecure TLS fallback.

## Topics

For device identifier `multibus-a1b2c3` and the default prefix:

- availability: `multibus/multibus-a1b2c3/status`
- channel state: `multibus/multibus-a1b2c3/channels/<id>/state`
- channel command: `multibus/multibus-a1b2c3/channels/<id>/set`
- event stream: `multibus/multibus-a1b2c3/events`

Availability is retained. The client publishes `online` after connecting and
uses retained `offline` as its MQTT last will.

State messages are retained when **Retain channel state** is enabled.

## State payload

State is JSON and includes the normalized Core binding:

```json
{
  "channel_id": 1,
  "source": "modbus",
  "point": "compat/1",
  "unit": "",
  "valid": true,
  "type": "float64",
  "value": 12.5
}
```

Possible `type` values are:

- `bool`
- `int64`
- `uint64`
- `float64`
- `text`

Only enabled ChannelRegistry bindings with a currently valid DataSource value
are published.

## Events

Core events are published immediately and are never retained. The MQTT consumer
uses an independent EventBus cursor, so publishing events does not interfere
with future LoRaWAN, Web UI or history consumers.

Example:

```json
{
  "sequence": 42,
  "timestamp": 1789761600,
  "severity": "warning",
  "source": "modbus",
  "type": "poll-failure",
  "detail": "slot=3"
}
```

The in-memory Core event ring is bounded. If MQTT remains unavailable long
enough for old events to be overwritten, the first subsequent event includes
`missed_before` with the number of events no longer available. The MQTT cursor
advances only after a successful publish.

## Commands

The client subscribes to one wildcard:

`<prefix>/<device>/channels/+/set`

The concrete topic is parsed back to a channel ID and checked against the live
ChannelRegistry. A command is accepted only when the binding exists, is enabled
and is marked writable.

Payload:

```json
{"value": 42}
```

The JSON type must match the DataPointDescriptor type. Accepted commands are
passed to the existing asynchronous `DataSource::writePoint()` implementation;
for Modbus this means the non-blocking write queue.

## Dynamic channels

The wildcard command subscription intentionally avoids per-channel broker
resubscription. Modbus compatibility channels created or removed at runtime are
therefore reflected automatically by the ChannelRegistry checks.

## Backup / restore

MQTT configuration is part of backup schema v3. Backup schema v2 remains
importable and restores MQTT as disabled/default-configured.

Backups currently contain credentials in clear text, like the existing Wi-Fi and
LoRaWAN secrets.

## Home Assistant discovery

Home Assistant MQTT discovery is optional and disabled by default. When enabled,
MultiBus publishes retained discovery configuration under the configured
discovery prefix (default `homeassistant`).

Entity mapping follows the Core descriptor:

- read-only Boolean -> `binary_sensor`
- read-only numeric/text -> `sensor`
- writable Boolean -> `switch`
- writable numeric -> `number`

Discovery reuses the same state, command and availability topics described
above. The discovery signature is recalculated from the active ChannelRegistry,
so newly created or modified channels are published without requiring a broker
reconnect.

## TLS

Enable **TLS with CA verification** and provide the broker's trust-anchor CA
certificate in PEM format. A TLS-enabled configuration without a CA certificate
is rejected.

The certificate is persisted with the device configuration and included in
configuration backups. It is public trust material rather than a private key,
but backups still contain MQTT/Wi-Fi/LoRaWAN credentials and must be protected.
