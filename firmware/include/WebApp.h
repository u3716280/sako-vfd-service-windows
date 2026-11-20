#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "ModbusRTU.h"
#include "VfdService.h"
#include "BackupService.h"
class WebApp {
public:
  WebApp(ModbusRTU& mb,VfdService& vfd,BackupService& backup):mb_(mb),vfd_(vfd),backup_(backup){}
  void begin();
  void tick();
private:
  WebServer server_{80};
  ModbusRTU& mb_;
  VfdService& vfd_;
  BackupService& backup_;
  bool controlArmed_{false};
  uint32_t controlArmedUntilMs_{0};
  bool controlArmed();
  void disarmControl();
  void sendJSON(int status, const String& json);
  void serveFile(const char* path, const char* mime);
  void initRoutes();
};
