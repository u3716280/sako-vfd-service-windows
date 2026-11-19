# v0.1 Validation Report — Source Package

- **Target:** Waveshare ESP32-S3-RS485-CAN (SKU 32154), 16MB flash, isolated RS-485.
- **Library:** 448 parameter entries loaded from previous manual audit; 445 have hexadecimal addresses; classifications: RW 309, RO 126, WO 8, Restricted 2, Unknown 3.
- **Modbus code:** read function `0x03` only; no `0x06` VFD write operation in source.
- **Dashboard:** U0 group scaling as stated in main manual; raw fault and state registers in protocol PDF; Estimated RPM from P1-04/P1-05.
- **Frontend:** HTML/CSS/JS included in LittleFS, profile local browser import/export, incremental backup API.
- **Static validation:** `python tests/validate_project.py` **PASS**. `node --check data/js/app.js` **PASS**.
- **Limitations:** PlatformIO/Arduino build was NOT possible in the authoring container (no PlatformIO board packages installed; offline connectivity). **No `.bin` image, bench-test or physical VFD test** is claimed. Board pin/DE direction should be verified on Waveshare schematic/revision; protocol PD/Fd discrepancy and returned scaling must be confirmed live.
- **Safety:** no VFD write or RUN/STOP; ESP32 Modbus serial configuration can be saved locally; deployed Wi-Fi AP password must be changed.

## Suggested commissioning checks

1. Build project with PlatformIO. If flash/PSRAM parameters differ on board revision, adjust `platformio.ini` accordingly.
2. Upload Firmware and Upload Filesystem Image; confirm `/api/library` returns 448.
3. Verify dashboard is Offline with RS-485 unplugged; ensure no fabricated values.
4. Verify Waveshare TX LED and RX LED, RS-485 GPIO17/18/21 and correct A/B wiring.
5. Read U0-00/U0-02/U0-03/U0-04 and compare against VFD front panel.
6. Read P1-04 and P1-05, validate Estimated RPM labeling.
7. Backup real device, verify failed entries, completeness flag, and JSON parse.
8. Confirm no write frames (Modbus function `0x06`) are ever transmitted by v0.1.
