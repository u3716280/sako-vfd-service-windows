# v0.1.1 — LittleFS partition label hotfix

## Symptom

After the filesystem image is uploaded successfully, Serial Monitor says:

```
E (...) esp_littlefs: partition "spiffs" could not be found
ERROR: LittleFS missing. Upload filesystem image from PlatformIO.
```

## Cause

`partitions.csv` names the data partition `littlefs`, but the original `LittleFS.begin(false)` looks for the default partition label `spiffs`.

## Fix

`src/main.cpp` now explicitly mounts the partition named `littlefs`:

```cpp
LittleFS.begin(false, "/littlefs", 10, "littlefs")
```

No partition layout changes were made. **Upload firmware only** after this fix if the previous filesystem upload succeeded. Do not format the filesystem.

```powershell
pio run -e waveshare_esp32s3_rs485_can -t upload --upload-port COM6
pio device monitor -p COM6 -b 115200
```

COM6 is from the user's example; it can change after reconnecting. If using your already-customized project, patch just the one `LittleFS.begin` line rather than replacing your whole project.

## Verification

- Reboot device after upload.
- Visit `http://192.168.4.1` over AP `SAKO-Service-v01`.
- Verify `/index.html` and `/library/ski780_parameters.json` can be served.
- If the error persists, inspect whether the firmware flashed and read the serial boot output.
