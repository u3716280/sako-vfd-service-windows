# SAKO SKI780 Portable VFD Service Tool — v0.1 Integrated MVP

**Hardware:** Waveshare **ESP32-S3-RS485-CAN**, onboard antenna, SKU 32154  
**Development:** VS Code + PlatformIO / Arduino C++  
**Purpose:** Temporary field-service tool for **read-only** monitoring, Parameter Library browsing and JSON backup over RS-485 Modbus RTU. **No VFD write command exists in this firmware.**

## 1. What's included

| Screen | Scope |
| --- | --- |
| Dashboard | Live Output Voltage, Output Current, Running Frequency, DC Bus, Estimated RPM, RUN/STOP, Forward/Reverse, Fault |
| Parameters | Search/filter full draft SKI780 library; inspect Default, Min, Max, Unit, Description, RO/RW/WO/Restricted/Unknown, read selected raw value |
| Parameter Library | 448 definitions from uploaded manuals; browse access breakdown and download full JSON |
| Profile Library | Import/export JSON profiles stored **in current browser** (localStorage), no VFD write |
| Backup | Sequential read-only Modbus backup with per-register success/failure, JSON download |
| Settings | ESP32's own baud, serial format, and slave address stored in Preferences |

**Excluded:** Input Voltage AC (no verified native register), RUN/STOP control, parameter editing, Compare Engine, Restore Engine. The UI never displays invented telemetry.

## 2. First setup / PlatformIO flash

1. Install **Visual Studio Code** and the **PlatformIO IDE** extension. Connect Waveshare using a **data-capable USB-C cable**.
2. Open this entire directory in VS Code (the directory containing `platformio.ini`). PlatformIO downloads board tools on first build, requiring Internet on your PC.
3. **Important security:** Copy `include/WiFiSecrets.h.example` to the ignored `include/WiFiSecrets.h`, then set a private `AP_WIFI_PASSWORD` (8+ characters). Never commit the populated file.
4. PlatformIO toolbar: **Build**. Select the `waveshare_esp32s3_rs485_can` environment.
5. **Upload** to flash firmware. If the port does not appear, see the Waveshare BOOT/RESET recovery instructions.
6. **Upload Filesystem Image** (PlatformIO: Project Tasks → `waveshare_esp32s3_rs485_can` → Platform → **Upload Filesystem Image**). This uploads `data/index.html`, `data/css/`, `data/js/`, and the 448-entry library JSON to **LittleFS**.
7. Open **Serial Monitor at 115200 baud**, note the printed AP address (normally `192.168.4.1`).
8. On phone/PC connect to Wi-Fi SSID `SAKO-Service-v01`, enter the password you set, then open `http://192.168.4.1`.
9. First run **with no VFD connected**: Web UI should load and all monitoring values should show `—`/Offline; Parameter Library should list **448** definitions.
10. Connect a powered VFD safely, select the matching serial settings, and verify live readings against the VFD display before trusting scaling.

**Two distinct uploads are necessary:** Firmware Upload and **Upload Filesystem Image**. If only firmware is uploaded, HTML and Library may be missing. Filesystem flashing may erase previous on-device backups. Download JSON before uploading a new image.

## 3. RS-485 wiring

| Waveshare RS-485 terminals | SAKO SKI780 |
| --- | --- |
| **A+** | **A+** / matching terminal verified at actual inverter |
| **B−** | **B−** / matching terminal verified at actual inverter |

Board power: USB-C 5V (power bank / USB) OR board DC terminals 7–36V as specified by Waveshare. **Never feed 24V into USB-C.** RS-485 A/B carries differential signals, not motor voltage. Avoid the VFD power wiring, motor output terminals, exposed high voltage and incorrect ground connections. RS-485 bus isolation is already onboard. Termination (120Ω) is jumper-selectable; use only when necessary for bus geometry.

**Waveshare internal UART map used in code:** TX=GPIO17, RX=GPIO18, Direction GPIO21 (HIGH TX/LOW RX). The hardware has an isolated transceiver; this pin map is referenced by board-specific community examples. **Verify with Waveshare's schematic/example for your board revision before live test**. For a communication failure, compare the board's RS-485 TX/RX LEDs with serial logs.

Waveshare documentation: https://docs.waveshare.com/ESP32-S3-RS485-CAN  
Waveshare Arduino examples and schematics: https://docs.waveshare.com/ESP32-S3-RS485-CAN/Resources-And-Documents

## 4. Modbus and scaling

SKI780 protocol: **Modbus RTU slave, 16-bit reads Function 0x03**, default 9600 **8N2**, Slave 1, Standard Modbus response needed by this implementation. **Non-standard Modbus replies can fail CRC/length checks**. The uploaded manuals have a naming discrepancy: main manual uses `PD-*`, protocol PDF uses `Fd-*`; verify the physical firmware's communication parameter names and Standard Modbus setting. **This tool will NOT auto-change any VFD parameter**.

Dashboard uses main-manual U0 monitor group because it defines display resolution:

| Display | Register | Raw scale | Data type |
| --- | --- | --- | --- |
| Running Frequency | 0x7000 / U0-00 | × 0.01 Hz | RO |
| DC Bus Voltage | 0x7002 / U0-02 | × 0.1 V | RO |
| Output Voltage | 0x7003 / U0-03 | × 1 V | RO |
| Output Current | 0x7004 / U0-04 | × 0.01 A | RO |
| RUN/STOP/Direction | 0x3000 | 1=Forward, 2=Reverse, 3=Stop | RO |
| Fault | 0x8000 | Raw fault code | RO |
| Motor rated frequency | P1-04 (0xF104) | × 0.01 Hz | RW in library, **read-only in v0.1** |
| Motor rated RPM | P1-05 (0xF105) | × 1 rpm | RW in library, **read-only in v0.1** |

Estimated RPM = Rated RPM × Output Hz / Rated Hz. **Not tachometer feedback; not guaranteed actual shaft speed.** During STOP/coasting, estimate reflects reported output frequency, not independently sensed shaft motion. Some scaling and VFD firmware compatibility remain **hardware-unverified**.

Monitoring polls one register approximately every 105 ms (~0.8–1.5s per complete cycle, depending on response timeout). Values older than 3.3 seconds disappear from the dashboard. **Do not use this dashboard for safety interlocks or proof of zero speed.**

## 5. Parameter Library / JSON backup rules

The supplied draft `data/library/ski780_parameters.json` has **448 entries** from two user-supplied documents (`SAKO_SKI780_Manual.pdf`, `Sako 485 Protocol .pdf`), metadata including Default/Min/Max/Unit/Description and an Access classification. Audit CSV is under `docs/Parameter_Audit_v1.csv`. Not all definitions are verified against the hardware.

The built-in `include/ParameterIndex.h` is auto-generated from the same library (445 entries with usable hexadecimal addresses, excluding 3 without verified address). **The code reads only parameters classified RO or RW.** WO / Restricted / Unknown are skipped and cannot be forced by uploaded JSON.

The sequential backup creates a JSON with `parameters` successful reads, `failed` list, `counts`, and a `complete` flag. Reads are non-atomic: the VFD can change while the scan runs. This is a partial service snapshot, not a synchronized register image. If even one readable register fails, `complete=false`.

JSON time is supplied by the browser clock (may be wrong, stored as `created_at_client`); no NTP/Internet is required. Backups are held on ESP32 LittleFS only for download and are **not** a reliable permanent archive: save copies on phone/PC.

**Restore policy for future v0.3:** only RW, with in-firmware trusted definitions and range/state verification. **v0.1 includes no Modbus Write Single Register 0x06.**

## 6. Service endpoints

| Method | Endpoint | Purpose |
| --- | --- | --- |
| GET | `/` | Main Web UI |
| GET | `/api/monitor` | Fresh telemetry (null if stale) |
| GET | `/api/status` | Modbus and device configuration/status |
| GET | `/api/library` | Parameter Definition Library JSON |
| GET | `/api/parameter?code=P1-04` | Read one RO/RW register into raw value |
| GET | `/api/backup/status` | Progress and results |
| POST | `/api/backup/start?time=...` | Start read-only full backup |
| POST | `/api/backup/cancel` | Cancel backup |
| GET | `/api/backup/download` | Download latest in-session completed JSON |
| POST | `/api/settings` | Change ESP32 slave/baud/format **only** |

Frontend uses standard Fetch polling, not WebSocket: approximately 1 sec for dashboard and 0.8 sec for backup progress. This avoids external embedded web libraries in v0.1 and works offline.

## 7. Troubleshooting

| Symptom | Check |
| --- | --- |
| Wi-Fi visible, page 404 | Run **Upload Filesystem Image**; verify LittleFS mounted in Serial Monitor |
| Dashboard Offline | VFD power; A/B wiring; correct Slave ID, Baud, Data Format; Standard vs Nonstandard Modbus; GPIO21 control |
| Timeout | A/B, slave address, wiring; inspect RS485 TX/RX LEDs; verify board RS-485 connector rather than CAN H/L |
| CRC mismatch | Check 8N2/8E1; Standard Modbus setting; RF / inverter EMI or incorrect transceiver direction |
| Some values show `—` | Missing/unsupported registers or reading failed; values hide after 3.3 seconds |
| Backup finishes partial | Open exported `failed` entries; some parameter groups may be unavailable per VFD model/firmware |
| Rated RPM blank | P1-04/P1-05 not readable or zero/unset; no guessed RPM shown |
| Can't connect on mobile | Connect directly to `SAKO-Service-v01`; disable mobile auto-switch to cellular if required |
| Firmware uploads but Serial silent | Try native USB port settings, BOOT/RESET procedure; confirm serial monitor 115200 |

## 8. Known limitations and validation status

- **Generated source, not compiled on the actual board in this environment.** Hardware pins and serial direction logic require validation on Waveshare SKU 32154. The full PlatformIO build also requires the board packages to be downloaded on your machine.
- Firmware is **not production security hardened**: no additional HTTP login beyond the WPA2 AP password; do not expose it on public networks. Set a private AP password before flashing.
- Modbus Register Map is an engineering draft, **not authenticated live hardware data**. Verify actual firmware's group naming, read permissions and scaling.
- Read-only polling may still interfere with systems using the **same RS-485 bus**; use as a temporary single master only.
- LittleFS mounted with `formatOnFail=false` to avoid automatically destroying definitions.
- Backup, Parameter Library and web assets require 16MB partition layout in `partitions.csv` and a filesystem upload.
- CAN not used. No cloud, motor RUN/STOP, VFD write, RESTORE or comparison in v0.1.

## 9. Source code map

```
platformio.ini                     PlatformIO target (16MB flash, OPI PSRAM)
partitions.csv                     Firmware + LittleFS partitions
include/AppConfig.h                Wi-Fi AP and Waveshare GPIO configuration
include/ParameterIndex.h           Compact generated firmware read index
src/ModbusRTU.cpp                  Modbus 0x03 + CRC16, half-duplex UART
src/VfdService.cpp                 Telemetry/scaling and stale data
src/BackupService.cpp              Incremental read-only backup and JSON stream
src/WebApp.cpp                     HTTP API and static file service
src/main.cpp                       Init, AP Wi-Fi, event loop
data/index.html                    Responsive frontend
 data/css/style.css                Styles
 data/js/app.js                    Dashboard / parameter / profile / backup UI
 data/library/ski780_parameters.json Full parameter metadata
docs/Parameter_Audit_v1.csv         Engineering audit data
tests/validate_project.py          Host-side static checks
```

## 10. Sources

User-supplied manuals are the source for Parameter definitions. Board-specific references:
- https://docs.waveshare.com/ESP32-S3-RS485-CAN
- https://docs.waveshare.com/ESP32-S3-RS485-CAN/Arduino
- https://docs.platformio.org/en/latest/platforms/espressif32.html

Version **0.1.0 Integrated MVP source package** — prepared for field commissioning, not certified/validated with a live VFD yet.

## v0.1.1 LittleFS hotfix

`src/main.cpp` explicitly mounts partition `littlefs` rather than the Arduino default `spiffs`. This matches the custom `partitions.csv` and resolves `partition "spiffs" could not be found`. See `docs/LITTLEFS_FIX_v0.1.1.md`.

## v0.1.2 LAN Wi-Fi mode

Edit `include/WiFiSecrets.h` and add 2.4 GHz router SSID and password; the default empty SSID keeps AP-only mode. After flashing, the Serial Monitor shows `LAN URL: http://192.168.x.x`; reserve this address in the router DHCP settings. A fallback AP `SAKO-Service-v01` at `192.168.4.1` is enabled if no LAN connection is established in 15 s or if the connection remains lost for 20 s. Credentials must not be committed to GitHub; see `.gitignore`. This is a read-only prototype without HTTP authentication: keep it on a trusted local network and do not expose it to the internet. Upload firmware **and** LittleFS image to install the updated Settings UI.

For Git: `include/WiFiSecrets.h` is gitignored. Copy `include/WiFiSecrets.h.example` to `include/WiFiSecrets.h` after a clean clone.
