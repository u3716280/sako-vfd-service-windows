#pragma once
#include <Arduino.h>
// No application-level login; use trusted LAN only. Never expose HTTP to the Internet.
static constexpr const char* AP_SSID = "SAKO-Service-v01";
static constexpr int RS485_RX_PIN = 18;  // Waveshare ESP32-S3-RS485-CAN
static constexpr int RS485_TX_PIN = 17;
static constexpr int RS485_DE_PIN = 21;  // HIGH TX, LOW RX; verify exact board revision
static constexpr uint32_t MODBUS_RESPONSE_TIMEOUT_MS = 180;
static constexpr uint32_t VFD_POLL_INTERVAL_MS = 105;
static constexpr uint32_t SAMPLE_FRESH_MS = 3300;
static constexpr const char* FW_VERSION = "0.2.0";
static constexpr const char* PRODUCT_ID = "sako-ski780-service-tool";
static constexpr const char* API_VERSION = "1.0";
