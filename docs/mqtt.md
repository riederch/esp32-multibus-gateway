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

Current transport is plain MQTT/TCP. TLS/certificate configuration is not yet
implemented and should be added before using MQTT across untrusted networks.

## Topics

For device identifier `multibus-a1b2c3` and the default prefix:

- availability: `multibus/multibus-a1b2c3/status`
- channel state: `multibus/multibus-a1b2c3/channels/<id>/state`
- channel command: `multibus/multibus-a1b2c3/channels/<id>/set`

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

## Remaining optional work

Home Assistant MQTT discovery remains an optional backlog item.
