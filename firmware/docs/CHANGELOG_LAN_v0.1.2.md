# v0.1.2 LAN Wi-Fi update

**Board:** Waveshare ESP32-S3-RS485-CAN (onboard antenna)

**Changes**
- Prefer Wi-Fi station mode using credentials set in include/WiFiSecrets.h; DHCP IP from router.
- AP fallback after 15 seconds on boot, or 20 seconds after LAN disconnect.
- Show LAN/AP address and mode in Settings and `/api/status`.
- Keep the existing `LittleFS.begin(false, "/littlefs", 10, "littlefs")` fix.
- No changes to Modbus register read/backup logic; no Modbus write path.

**Installation**
1. Fill in include/WiFiSecrets.h with your 2.4 GHz Wi-Fi credentials.
2. Set a private `AP_WIFI_PASSWORD` in the ignored include/WiFiSecrets.h.
3. `pio run -e waveshare_esp32s3_rs485_can -t upload --upload-port COM6`
4. `pio run -e waveshare_esp32s3_rs485_can -t uploadfs --upload-port COM6`
5. Monitor at 115200, use LAN IP shown. Verify `/api/status` fields.

**Known limits**
- No HTTPS or application-level login yet, so trusted LAN only.
- On Wi-Fi AP+STA fallback, the AP may remain active after STA reconnects until a reboot.
- The actual board has not been flashed or runtime-tested in this environment.
