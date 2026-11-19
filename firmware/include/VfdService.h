#pragma once
#include <Arduino.h>
#include "ModbusRTU.h"
struct VfdSample { uint16_t raw=0; uint32_t stamp=0; bool known=false; };
class VfdService {
public:
  explicit VfdService(ModbusRTU &bus):bus_(bus){}
  void tick(bool paused);
  String toJson(bool paused) const;
private:
  ModbusRTU &bus_;
  uint32_t lastPoll_=0;
  uint8_t cursor_=0;
  VfdSample sample_[8];
  static constexpr uint16_t REGISTERS_[8]={0x7000,0x7002,0x7003,0x7004,0x3000,0x8000,0xF104,0xF105};
  bool fresh(size_t index) const;
};
