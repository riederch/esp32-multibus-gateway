# Security, provisioning and backup/restore

## Scope

This document defines the initial provisioning flow, administrator authentication, physical factory reset and configuration backup/restore behaviour of the MultiBus Gateway.

## Initial provisioning

On first boot the device starts in commissioning mode and creates two independent random credentials:

1. **AP password** for the temporary configuration WLAN
2. **Initial administrator password** for the Web UI

The display shows the temporary AP credentials and the initial administrator password during first commissioning.

Example:

```text
FIRST SETUP

WiFi: MultiBus-A31F
PW: K7QP-4M2X

Admin:
W8N4-P6RK-T2

192.168.4.1
```

The initial administrator password is valid only until the administrator completes the first login and sets a new password.

## Mandatory password change

The first successful Web UI login using the generated initial administrator password must force a password change before any normal administration function is available.

After the change:

- the initial password is invalidated permanently
- the initial password is no longer displayed
- the new administrator password is stored only as a salted password hash
- no API or diagnostic interface may expose the clear-text administrator password

## Wi-Fi commissioning mode

The device supports two Wi-Fi modes:

### Client mode

The device connects to a configured WLAN and exposes the Web UI through its assigned IP address and, where available, mDNS hostname.

The display should show at least:

- friendly device name
- WLAN connection state
- IP address
- mDNS hostname

The Web UI should use the standard HTTP/HTTPS port where practical so that no explicit port number is required in normal use.

### Access-point mode

If the device is not configured for a WLAN, cannot connect during commissioning, or commissioning mode is manually requested, it starts its own access point.

The display shows:

- AP SSID
- AP password
- configuration IP address
- optional mDNS/captive-portal hint

A captive portal may be provided as a convenience, with the displayed IP address remaining the deterministic fallback.

## Physical button behaviour

The user button is part of the general platform UI. It may be used for normal short-press functions such as changing display pages and for entering commissioning mode.

### Factory reset

There is **no separate administrator-password-reset function on the physical button**.

A sufficiently long deliberate button action performs a **complete factory reset** only.

The exact timing may be tuned during implementation; the intended interaction is approximately:

```text
hold button for about 10 seconds
        -> display shows factory-reset warning/countdown
continue holding until confirmed
        -> full factory reset
        -> reboot into first-setup mode
```

A factory reset removes all mutable device configuration, including:

- administrator credentials
- Web UI sessions/tokens
- Wi-Fi client configuration
- generated AP credential
- component configuration (V/L/M/G)
- LoRaWAN credentials and settings
- Modbus channels and mappings
- Victron component configuration
- GNSS configuration
- automation rules
- alarm configuration
- user variables
- local history and retransmission state
- MQTT/network integration settings where present
- imported certificates/keys and other user-managed secrets

Immutable hardware identity and the installed firmware image remain intact unless the platform specifically requires otherwise.

After reset, the device behaves exactly like a newly provisioned unit and generates new temporary commissioning credentials.

## Backup and restore

The Web UI/backend provides explicit configuration backup download and upload functions.

### Backup download

An authenticated administrator can create and download a backup archive from the Web UI.

Suggested backend operation:

```text
GET /api/system/backup
```

or an equivalent authenticated action.

The backup contains the complete portable operational configuration required to recreate the device setup, including:

- system/device configuration
- V/L/M/G component enable/mode state
- Wi-Fi configuration
- LoRaWAN configuration and credentials
- Modbus serial settings
- Modbus channels/register definitions
- Modbus slave mappings where applicable
- Victron component settings
- GNSS settings
- automation rules
- alarms and thresholds
- display/UI preferences
- reporting/retransmission configuration
- MQTT/network integration configuration where present

The backup should include a schema version and metadata such as firmware version, hardware/board type and export timestamp.

### Security of backup files

Backups may contain operational secrets such as Wi-Fi passwords, LoRaWAN keys and integration credentials. Therefore the portable backup format should support encryption using a backup password supplied by the administrator at export time.

The administrator password itself is **not portable configuration** and should not be restored from a backup. Active Web UI sessions/tokens are never exported.

On restore, the currently configured administrator authentication remains in effect. On a fresh/factory-reset device, restore is performed only after completing initial administrator provisioning.

### Backup upload / restore

An authenticated administrator can upload a previously exported backup through the Web UI.

Suggested backend operation:

```text
POST /api/system/restore
```

The backend must validate the backup before applying it:

1. archive/format integrity
2. schema version
3. supported hardware/board type
4. configuration ranges and enum values
5. component/capability compatibility
6. optional cryptographic integrity/authentication

The restore process should be transactional:

```text
upload
 -> validate completely
 -> stage configuration
 -> apply only if valid
 -> persist atomically
 -> reboot if required
```

An invalid or incompatible backup must not partially overwrite the active configuration.

## Backup compatibility and migration

The configuration format must be versioned independently of the firmware version.

Example:

```json
{
  "schema_version": 1,
  "device_family": "esp32-multibus-gateway",
  "board": "HTIT-WB32LAF-V4.2",
  "firmware_version": "0.1.0"
}
```

Future firmware versions should provide configuration migrations where practical so older backups can be restored safely.

## Security principles

- no universal/default administrator password
- no hidden master password
- no physical admin-password-only reset path
- full physical reset is the only local credential-reset mechanism
- administrator password stored only as a salted hash
- login rate limiting / temporary lockout
- authenticated backup/restore endpoints
- backups containing secrets should be encrypted
- sessions invalidated after relevant security/configuration changes
- destructive operations require explicit Web UI confirmation
