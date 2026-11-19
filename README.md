# SAKO SKI780 VFD Service Tool

Checkpoint repository for a native Windows service application and ESP32 firmware used with the SAKO SKI780 VFD.

## Current checkpoint

- Windows application baseline: **v0.1**
- ESP32 firmware baseline: **v0.1.2 LAN/Wi-Fi**
- Target board: Waveshare ESP32-S3-RS485-CAN (SKU 32154)
- VFD communication: isolated RS-485 / Modbus RTU

This checkpoint was recovered from the last persistent source archives. It is intentionally pushed before reconstructing and validating the integrated v0.1.3 release.

## Repository layout

- `windows/` — native WPF Windows application
- `firmware/` — PlatformIO ESP32 firmware and LittleFS web assets
- `.github/workflows/` — Windows build workflow

## Safety

The current firmware baseline is intended for monitoring and backup workflows. Do not use it to issue RUN/STOP commands or restore/write VFD parameters until those operations have been explicitly implemented and validated.

