#pragma once
#include <Arduino.h>
#include <FS.h>
#include "ModbusRTU.h"
class BackupService {
public:
  explicit BackupService(ModbusRTU& bus):bus_(bus){}
  bool start(const String& browserTime);
  void tick();
  bool active() const {return active_;}
  bool ready() const {return ready_;}
  String statusJson() const;
  void cancel();
  static constexpr const char* OUTPUT_PATH = "/backup.json";
private:
  ModbusRTU& bus_;
  File vals_, errs_;
  bool active_{false}, ready_{false};
  bool haveVal_{false},haveErr_{false};
  size_t next_{0},total_{0},processed_{0},ok_{0},failed_{0},skipped_{0};
  String clientTimestamp_, stage_{"idle"};
  void finish();
  static void copyContent(File& from, File& into);
  static String safeJSON(const String &s);
};
