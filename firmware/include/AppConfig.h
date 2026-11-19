#pragma once
#include <Arduino.h>
// Change this before flashing! v0.1.2 has no application-level login; use trusted LAN only.
static constexpr const char* AP_SSID = "SAKO-Service-v01";
static constexpr int RS485_RX_PIN = 18;  // Waveshare ESP32-S3-RS485-CAN
static constexpr int RS485_TX_PIN = 17;
static constexpr int RS485_DE_PIN = 21;  // HIGH TX, LOW RX; verify exact board revision
static constexpr uint32_t MODBUS_RESPONSE_TIMEOUT_MS = 180;
static constexpr uint32_t VFD_POLL_INTERVAL_MS = 105;
static constexpr uint32_t SAMPLE_FRESH_MS = 3300;
static constexpr const char* FW_VERSION = "0.1.2";
