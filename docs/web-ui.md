# Web UI and local interaction

## Primary administration path

Wi-Fi + Web UI is the primary configuration and administration interface.

## Wi-Fi client mode

When WLAN credentials are configured, the device joins the configured network and exposes the Web UI.

The OLED should show:

- friendly device name
- WLAN connection state
- assigned IP address
- mDNS hostname where available

Prefer standard HTTP/HTTPS ports so an explicit port does not normally need to be displayed.

## AP commissioning mode

On first setup, or when commissioning mode is deliberately requested, the device starts an access point.

The OLED shows:

- SSID
- randomly generated per-device AP password
- configuration IP address
- optional captive-portal/mDNS hint

A captive portal may be implemented as a convenience; the displayed IP remains the deterministic fallback.

## Initial administrator credential

On first boot the device creates a one-time initial administrator password and shows it on the OLED together with commissioning information.

The first successful login must force the administrator to select a new password before normal administration continues.

After that, the initial password is invalid and is never displayed again.

## Web UI sections

Target navigation:

```text
Dashboard
|- System
|  |- Status
|  |- Hardware
|  |- Firmware
|  |- Logs / diagnostics
|  `- Time
|- Components
|  |- Victron
|  |- LoRa
|  |- Modbus
|  `- GNSS
|- Automation
|  |- Rules
|  |- Alarms
|  `- Variables
|- Data
|  |- Channels
|  |- History
|  `- Communication diagnostics
|- Network
|  |- Wi-Fi
|  |- LoRaWAN
|  `- MQTT where implemented
`- Maintenance
   |- Backup download
   |- Backup restore/upload
   |- Firmware update
   |- Reboot
   `- Factory reset
```

## Component configuration

The UI should directly represent the component state model:

```text
Victron
  enabled / disabled

LoRa
  LoRaWAN
  Meshtastic [not implemented / backlog]
  disabled

Modbus
  master
  slave
  disabled

GNSS
  enabled / disabled
```

Invalid or unsupported combinations must be rejected with a clear capability/resource reason.

## Dashboard

Show active component state and key health indicators, for example:

```text
Victron     active / online
LoRa        LoRaWAN / joined
Modbus      master / 2 of 2 devices online
GNSS        fixed / satellites
Wi-Fi       connected
Power       source / battery state
```

## OLED pages

Planned pages:

1. System: name, firmware, uptime
2. Network: Wi-Fi, IP, mDNS
3. LoRa: backend, join state, signal/diagnostics
4. Modbus: mode, device/channel health, errors
5. Victron: status and key values when enabled
6. GNSS: fix, satellites, position when enabled
7. Power: source, BAT/SOL status where available
8. Alarm/diagnostic page when attention is required

## User button

- short press: cycle pages or context action
- deliberate long press: complete factory reset with visible warning/countdown
- no admin-password-only reset action
- commissioning-mode gesture may be defined separately during implementation as long as it cannot be confused with factory reset

## Backup/restore

Maintenance UI provides authenticated:

- backup download
- backup upload/restore

Restore is validated before application and must not partially overwrite active configuration.

## Firmware update

Primary in-field update path: Web UI / Wi-Fi OTA.

USB-C remains available for initial flashing, service and recovery.

## BLE relationship

BLE is not required for normal MultiBus administration. When `V=1`, the Victron cocoon may use BLE for VictronConnect/Smart-Dongle compatibility without changing the platform Web UI model.
