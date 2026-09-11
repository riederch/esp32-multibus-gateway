# ESP32 MultiBus Gateway - Handover

Stand: 2026-09-11

## Repository

- Repository: `riederch/esp32-multibus-gateway`
- Branch: `main`
- Basis dieses Handovers: `06ef6e593efc5b17de87f71596d8effcdef20438`
- CI auf dieser Basis: erfolgreich
- Build-System: PlatformIO / Arduino-ESP32
- Build-Environment: `heltec-v4`

## Ziel

MultiBus Gateway ist ein modulares ESP32-S3-Feldgateway auf Basis des Heltec HTIT-WB32LAF V4.2 / WiFi LoRa 32 V4.2. Es verbindet LoRaWAN, RS485/Modbus, Victron VE.Bus, GNSS, lokale Administration und Automatisierung über ein gemeinsames Source-/Channel-/Event-/Command-Modell.

## Komponentenmodi

```text
V = [1,0]       Victron ein / aus
L = [W,M,0]     LoRaWAN / Meshtastic / aus
M = [M,S,0]     Modbus Master / Slave / aus
G = [1,0]       GNSS ein / aus
```

`L=W` und `L=M` sind gegenseitig ausgeschlossen, da beide den onboard SX1262 verwenden.

## Architektur

Die verbindliche Abstraktion lautet:

```text
                         CORE
                          |
       +------------------+------------------+
       |                  |                  |
   DataSources        Consumers          Transports
       |                  |                  |
   Modbus             Rules              LoRa
   Victron            History              |
   GNSS               Web UI          LoRaWAN / Mesh
   Platform I/O
```

Wichtige Invarianten:

- LoRa ist ein Transport direkt am Core und hängt nicht an Victron oder VE.Bus.
- Modbus, Victron, GNSS und Platform-I/O liefern normalisierte Datenpunkte.
- Victron wird intern nicht als Fake-Modbus-Slave modelliert.
- Channels abstrahieren Quelle, Reporting, Alarm, History und Write-Policy.
- Native Protokollframes bleiben in den jeweiligen Komponenten.

## Firmwarestatus

| Bereich | Status | Anmerkung |
|---|---|---|
| Application Lifecycle | implementiert | lädt Config, Security, Board, Komponenten, Netzwerk und WebUI |
| ConfigStore / NVS | implementiert | persistente Konfiguration und Schema-Migration |
| Component Modes V/L/M/G | implementiert | Konfiguration und Validierung vorhanden |
| SecurityStore | Basis implementiert | Initialpasswort, Hashing, Login-Prüfung; Hardening bleibt offen |
| Wi-Fi Client | implementiert | gespeicherte Konfiguration |
| AP-Fallback | implementiert | Commissioning-Fallback vorhanden |
| WebUI | Basis implementiert | Login, Komponenten, WLAN, Status, Passwortwechsel, Reboot |
| Backup / Restore | implementiert | aktueller Export enthält Secrets noch unverschlüsselt |
| Factory Reset | implementiert | langer User-Button-Press, kein separater Passwortreset |
| ChannelRegistry | implementiert | interne Channels + Compatibility-Slot-Mapping |
| LoRaWAN Identity | Basis implementiert | DevEUI/JoinEUI/AppKey-Konfiguration und Validierung |
| FPort-85 Codec | teilweise implementiert | Modbus-Channel-Konfiguration, Periodic Value und Collection Exception |
| FPort-85 Tests | implementiert | Host-Test in CI |
| Modbus Channel Store | implementiert | Compatibility-Channels persistent in NVS |
| Modbus RTU Master | noch nicht implementiert | echte UART/RS485-Abfrage und `readPoint()` fehlen |
| Modbus RTU Slave | noch nicht implementiert | virtuelle Registermap fehlt |
| LoRaWAN Funkstack | noch nicht implementiert | SX1262 Join/Uplink/Downlink fehlen; `send()`/`connected()` sind noch Stubs |
| Victron VE.Bus | noch nicht implementiert | Component/DataSource-Struktur vorhanden, Protokollengine fehlt |
| Victron USB MK3 | noch nicht implementiert | spezifiziert |
| Victron BLE | noch nicht implementiert | spezifiziert |
| GNSS | noch nicht implementiert | DataSource-Struktur vorhanden |
| Rule Engine | spezifiziert | Laufzeitimplementierung fehlt |
| History | spezifiziert | Ringbuffer/Store-and-forward fehlt |
| OLED UI | spezifiziert | reale Display-Ausgabe fehlt |
| Web OTA Upload | noch nicht implementiert | Firmware-Artefakte werden bereits erzeugt |

## LoRaWAN

### Identität

OTAA ist der Primärmodus.

- `DevEUI`: stabile 64-Bit-Geräteidentität; für Entwicklung aus der ESP32-Factory-Identity ableitbar, Produktion muss provisionierte EUI-64 unterstützen.
- `JoinEUI`: konfigurierbarer 64-Bit-Wert für den Join-/Application-Domain.
- `AppKey`: individueller zufälliger 128-Bit-Root-Key pro Gerät.
- ChirpStack Application UUID ist Server-Metadatum und kein LoRaWAN-Air-Interface-Identifier.

### Protokoll

- Compatibility Profile: FPort 85
- Ziel: byte- und semantikkompatibel zum anwendbaren Milesight-UC100-V2-LoRaWAN-Protokoll, ausgenommen proprietäres D2D.
- MultiBus-Erweiterungen verwenden einen separaten konfigurierbaren FPort.
- FPort 85 darf niemals mit inkompatiblen MultiBus-Kommandos erweitert werden.

Aktuell implementiert/verifiziert im Codec:

```text
FF 03   Reporting-Prefix erkannt
F9 78   RS485-Config-Prefix erkannt
F9 79   Modbus-Global-Config-Prefix erkannt
FF EF   Modbus Channel create/update/delete/name dekodiert
F9 73   Periodic Channel Value Encoder
FF 15   Collection Exception Encoder
```

Compatibility-Slots:

```text
Downlink Channel ID 1..32
        <->
interner Compatibility Slot 0..31
```

Die vollständige UC100-Kompatibilität ist erst abgeschlossen, wenn sämtliche anwendbaren nicht-D2D-Kommandos einzeln implementiert und mit Binärvektoren getestet wurden.

## Modbus

Aktuell vorhanden:

- persistente `ModbusChannelConfig`
- 32 UC100-kompatible Slots
- Mapping in `ChannelRegistry`
- Datentyp-/Sign-/Quantity-Modell
- FPort-85-Channel-Konfiguration

Nächster Firmware-Meilenstein:

```text
RS485 UART
  -> Modbus RTU Master
  -> Polling
  -> DataSource::readPoint()
  -> ChannelRegistry
  -> F9 73 Encoder
  -> LoRaWAN Transport
```

## Hardware Rev A

Referenzboard:

- Heltec HTIT-WB32LAF V4.2 / WiFi LoRa 32 V4.2
- ESP32-S3
- SX1262 EU868

### Feldanschluss J1

```text
1 V+   externe Versorgung 5-30 V DC
2 V-   Versorgung / CORE_GND
3 A    Modbus RS485 A
4 B    Modbus RS485 B
```

Bevorzugter Steckverbinder: Phoenix Contact `1757268`, MSTBA 2,5/4-G-5,08.

### VE.Bus J2

Bevorzugter RJ45: Würth `615008143721`.

```text
1 NC
2 VEBUS_VPLUS
3 VEBUS_GND
4 VEBUS_A
5 VEBUS_B
6 VEBUS_STB
7 VEBUS_PD
8 NC
```

### GPIO Rev A

```text
Modbus RX   GPIO2
Modbus TX   GPIO4
Modbus DIR  GPIO5

VE.Bus RX   GPIO47
VE.Bus TX   GPIO48
VE.Bus DIR  GPIO6

VE.Bus STB  GPIO43   optional / DNI
VE.Bus PD   GPIO44   optional / DNI
```

GPIO3, GPIO45 und GPIO46 bleiben wegen ESP32-S3-Strapping frei.

## Versorgung Rev A

Die Rohspannungen der externen Versorgung und des VE.Bus werden nicht zusammengeschaltet. Beide Quellen werden separat gewandelt und erst auf der Core-seitigen Niederspannung geORt.

```text
External 5-30 V
   -> protection
   -> TPS26600PWP
   -> LMR38020SDDAR
   -> EXT_5V
   -> D_EXT -----------+
                       |
                       +--> SYS_5V --> JP_USB_SAFE --> HELTEC_5V
                       |
VE.Bus V+ / GND        |
   -> protection       |
   -> isolated DC/DC   |
   -> VE_ISO_5V        |
   -> D_VE ------------+
```

Isolation:

- `VEBUS_GND` ist nicht `CORE_GND`.
- VE.Bus-Power kreuzt ausschließlich den isolierten U7-DC/DC-Pfad.
- VE.Bus-Daten kreuzen ausschließlich die U5-ISOW1412-Barriere.
- Modbus-Daten kreuzen ausschließlich die U3-ISOW1412-Barriere.
- Modbus- und VE.Bus-Isolationsdomänen bleiben voneinander getrennt.

### USB-Regel

Heltec V4.2 darf nicht gleichzeitig über USB und den externen 5-V-Pin gespeist werden.

```text
Feldbetrieb:          JP_USB_SAFE CLOSED
USB Service/Flashen:  JP_USB_SAFE OPEN
```

Silkscreen: `OPEN FOR USB`.

### Kritische Bauteile

```text
U1  TPS26600PWP       TI PWP0016A
U2  LMR38020SDDAR     TI DDA0008B
U3  ISOW1412DFM       TI DFM0020A   Modbus
U5  ISOW1412DFM       TI DFM0020A   VE.Bus
U4  SM712             Modbus TVS
U6  SM712             VE.Bus TVS
U7  isolated DC/DC    Auswahl nach VEBUS_VPLUS-Messung
```

Aktuelle Baselinewerte:

```text
TPS26600PWP
  R_ILIM   11.8 kOhm 1 %
  C_dVdT   22 nF
  UVLO     IN
  OVP      RTN
  MODE     RTN

LMR38020SDDAR
  fSW      400 kHz
  R_RT     64.9 kOhm 1 %
  R_FBT    100 kOhm 1 %
  R_FBB    23.7 kOhm 1 %
  L        15 uH, Isat >= 3 A
  C_BOOT   100 nF
  COUT     3 x 22 uF X7R

ISOW1412DFM
  VIO      3V3
  VDD      SYS_5V
  EN/FLT   4.7 kOhm pull-up to 3V3
```

`U1_RTN` darf nicht mit `CORE_GND` hart verbunden werden; diese Trennung ist Bestandteil des TPS26600-Verpolschutzes.

## KiCad / PCB Rev A

Verzeichnis:

```text
hardware/rev-a/
```

Wichtige Dateien:

```text
multibus-rev-a-detail.sch
    aktuelle detaillierte elektrische Arbeitsgrundlage

multibus-rev-a-placement-v2.kicad_pcb
    aktuelle Placement-Studie mit echten TI-Landpatterns

multibus-rev-a.pretty/
    lokale Footprint-Bibliothek

bom-critical.csv
    kritische BOM

pcb-footprints.md
    Footprint- und Layoutregeln
```

Verifizierte TI-Landpatterns sind vorhanden für:

```text
PWP0016A  TPS26600
DDA0008B  LMR38020
DFM0020A  ISOW1412
```

Weitere vorhandene Footprints:

- Heltec WiFi LoRa 32 V4.2 Carrier
- Phoenix 1757268
- Würth 615008143721
- universeller U7-Prototyp-Platzhalter

Aktuelle PCB-Arbeitsgröße:

```text
110 x 70 mm
```

Dieses Maß ist nicht eingefroren. Finale Außenkontur und Montagebohrungen werden vom Gehäuse bestimmt.

### Noch kein PCB-Release

Die aktuelle Tool-Umgebung besitzt kein `kicad-cli`. Deshalb wurden noch nicht durchgeführt:

- KiCad-Konvertierung auf aktuelle `.kicad_sch`
- ERC
- vollständiges Schematic-to-PCB-Netlisting
- Routing
- DRC
- Gerber-/Drill-/Pick&Place-Release

Die Placement-Dateien sind keine Fertigungsfreigabe.

## Offene Hardwaremessungen

Vor finaler Rev-A-Bestückung/Freigabe sind insbesondere zu messen bzw. zu prüfen:

1. `VEBUS_VPLUS` gegen `VEBUS_GND` in allen MultiPlus-Zuständen.
2. verfügbare VE.Bus-V+-Leistung / Source Impedance.
3. daraus Auswahl des isolierten U7-DC/DC-Wandlers.
4. VE.Bus A/B-Polarität, Idle/Common-Mode, Bias und Terminierung.
5. VE.Bus Turnaround-Timing bei 256000 baud.
6. STB/PD-Pegel und gewünschtes Verhalten.
7. D_EXT/D_VE Forward-Drop, Reverse-Leakage und Temperatur.
8. externe Versorgung 5-30 V, Reverse Polarity und Current Limit.
9. ESD/EFT/EMV und thermischer Betrieb.
10. finale Gehäusekontur und Montagebohrungen.

## Build / CI

Lokaler Firmware-Build:

```bash
pio run
```

FPort-85-Host-Test:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude tests/fport85_codec_test.cpp -o /tmp/fport85_codec_test
/tmp/fport85_codec_test
```

GitHub Actions führt beide Prüfungen bei Push/PR aus.

Bei erfolgreichem Build entstehen:

```text
firmware.bin
firmware.bin.sha256
```

als Actions-Artefakt. Tags `v*` erzeugen zusätzlich GitHub-Release-Assets.

## Priorisierte nächste Schritte

1. `multibus-rev-a-detail.sch` in aktuelles KiCad importieren und als `.kicad_sch` speichern.
2. das kompakte Heltec-Symbol für das Netlisting durch das geprüfte 36-Pin-Symbol ersetzen.
3. Footprints vollständig zuweisen und ERC ohne ungeklärte Fehler durchführen.
4. PCB aus dem Schaltplan aktualisieren und `placement-v2` als Basis verwenden.
5. VE.Bus-V+-Messung durchführen und U7 festlegen.
6. Isolation/RF/Power-Platzierung finalisieren, routen und DRC durchführen.
7. firmwareseitig echten Modbus-RTU-Master implementieren.
8. anschließend echten SX1262-LoRaWAN-Stack mit OTAA/Uplink/Downlink implementieren.
9. FPort-85-Kompatibilität Befehl für Befehl vervollständigen.
10. WebUI um LoRaWAN-Provisioning und OTA-Firmware-Upload erweitern.
11. danach Victron VE.Bus/MK3/BLE, GNSS, Rules und History implementieren.

## Nicht ändern ohne bewusste Architekturentscheidung

- LoRa bleibt Transport direkt am Core.
- Victron wird nicht als synthetisches Modbus-Gerät modelliert.
- FPort 85 wird nicht für inkompatible MultiBus-Erweiterungen verwendet.
- VE.Bus-GND wird nicht direkt mit CORE_GND verbunden.
- `U1_RTN` wird nicht direkt mit CORE_GND verbunden.
- USB-C und Heltec-5V werden nicht gleichzeitig betrieben; `JP_USB_SAFE` beachten.
- Modbus und VE.Bus behalten getrennte isolierte Busdomänen.

## Source of truth

Projekt:

- `README.md`
- `docs/specification.md`
- `docs/architecture.md`
- `docs/lorawan-protocol.md`
- `docs/security-and-provisioning.md`
- `docs/web-ui.md`
- `docs/rule-engine.md`
- `docs/validation.md`

Hardware:

- `docs/hardware.md`
- `docs/hardware-rev-a.md`
- `docs/power-topology-rev-a.md`
- `hardware/rev-a/README.md`
- `hardware/rev-a/bom-critical.csv`
- `hardware/rev-a/pcb-footprints.md`

Wenn sich Schaltplan, PCB-Studie und permanente Spezifikation widersprechen, darf Rev A nicht freigegeben werden, bevor die Abweichung aufgelöst ist.
