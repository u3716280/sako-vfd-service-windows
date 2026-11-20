#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "AppConfig.h"
#include "WiFiSecrets.h"
#include "ModbusRTU.h"
#include "VfdService.h"
#include "BackupService.h"
#include "WebApp.h"

static ModbusRTU modbus;
static VfdService vfd(modbus);
static BackupService backup(modbus);
static WebApp web(modbus,vfd,backup);

// Prefer existing LAN Wi-Fi (STA). If unavailable, enable a portable AP.
static bool fallbackApActive = false;
static uint32_t disconnectedSinceMs = 0;

static void startFallbackAP() {
  if(fallbackApActive) return;
  if(strlen(LAN_WIFI_SSID) > 0) WiFi.mode(WIFI_AP_STA);
  else WiFi.mode(WIFI_AP);
  if(!WiFi.softAP(AP_SSID, AP_WIFI_PASSWORD)) {
    Serial.println("ERROR starting fallback access point");
    return;
  }
  fallbackApActive = true;
  Serial.print("Fallback AP SSID: "); Serial.println(AP_SSID);
  Serial.print("Fallback AP URL: http://"); Serial.println(WiFi.softAPIP());
}

static void startNetwork() {
  WiFi.setHostname("sako-ski780"); // Must precede WiFi.mode/begin.
  WiFi.setSleep(false);
  if(strlen(LAN_WIFI_SSID) == 0) {
    Serial.println("LAN Wi-Fi not configured; using portable AP");
    startFallbackAP();
    return;
  }
  Serial.print("Connecting to LAN Wi-Fi SSID: "); Serial.println(LAN_WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(LAN_WIFI_SSID, LAN_WIFI_PASSWORD);
  const uint32_t startMs = millis();
  while(WiFi.status() != WL_CONNECTED && (millis()-startMs) < 15000UL) {
    delay(250);
  }
  if(WiFi.status() == WL_CONNECTED) {
    Serial.print("LAN URL: http://"); Serial.println(WiFi.localIP());
  } else {
    Serial.println("LAN connection unavailable; starting portable AP fallback");
    startFallbackAP();
  }
}

static void serviceNetwork() {
  if(strlen(LAN_WIFI_SSID) == 0 || fallbackApActive) return;
  if(WiFi.status() == WL_CONNECTED) {
    disconnectedSinceMs = 0;
    return;
  }
  if(disconnectedSinceMs == 0) disconnectedSinceMs = millis();
  if((millis()-disconnectedSinceMs) >= 20000UL) {
    Serial.println("LAN disconnected for 20 seconds; enabling fallback AP");
    startFallbackAP();
  }
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\nSAKO SKI780 Portable Service Tool v0.3.0 | GUARDED MOTOR CONTROL");
  // Never format silently: formatting could erase the definition library.
  if(!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
    Serial.println("ERROR: LittleFS missing. Upload filesystem image from PlatformIO.");
    // Allow AP and serial diagnostics even when the filesystem is missing.
  }
  Preferences preferences;preferences.begin("sako780",true);
  uint32_t baud=preferences.getUInt("baud",9600);
  uint8_t slave=preferences.getUChar("slave",1);
  String format=preferences.getString("format","8N2");
  preferences.end();
  if(baud==0) baud=9600;
  if(slave<1 || slave>247) slave=1;
  modbus.begin(baud,format,slave);
  startNetwork();
  web.begin();
}
void loop() {
  serviceNetwork();
  web.tick();
  if(backup.active()) backup.tick();
  else vfd.tick(false);
  delay(1);
}
