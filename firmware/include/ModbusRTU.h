#pragma once
#include <Arduino.h>
class ModbusRTU {
public:
  void begin(uint32_t baud, const String& format, uint8_t slave);
  bool readRegister(uint16_t addr, uint16_t &value);
  bool writeRegister(uint16_t addr, uint16_t value);
  uint8_t slave() const {return slave_;}
  uint32_t baud() const {return baud_;}
  const String& format() const {return format_;}
  const String& lastError() const {return error_;}
  uint32_t successfulReads() const {return successes_;}
  uint32_t failedReads() const {return failures_;}
  uint32_t successfulWrites() const {return writeSuccesses_;}
  uint32_t failedWrites() const {return writeFailures_;}
private:
  static uint16_t crc16(const uint8_t* bytes, size_t len);
  HardwareSerial bus_{1};
  uint8_t slave_{1};
  uint32_t baud_{9600};
  String format_{"8N2"};
  String error_{"not_started"};
  uint32_t successes_{0}, failures_{0};
  uint32_t writeSuccesses_{0}, writeFailures_{0};
};
