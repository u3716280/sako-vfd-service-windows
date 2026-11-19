#include "ModbusRTU.h"
#include "AppConfig.h"

uint16_t ModbusRTU::crc16(const uint8_t* bytes, size_t len) {
  uint16_t crc=0xFFFF;
  for(size_t i=0;i<len;++i) {
    crc ^= bytes[i];
    for(uint8_t b=0;b<8;++b) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
  }
  return crc;
}

void ModbusRTU::begin(uint32_t baud, const String& format, uint8_t slave) {
  baud_=baud; format_=format; slave_=slave;
  uint32_t mode=SERIAL_8N2;
  if(format=="8N1") mode=SERIAL_8N1;
  else if(format=="8E1") mode=SERIAL_8E1;
  else if(format=="8O1") mode=SERIAL_8O1;
  bus_.end();
  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW); // receive by default
  bus_.begin(baud_, mode, RS485_RX_PIN, RS485_TX_PIN);
  bus_.setTimeout(MODBUS_RESPONSE_TIMEOUT_MS);
  error_="no_response_yet";
}

bool ModbusRTU::readRegister(uint16_t addr, uint16_t& value) {
  // READ-ONLY. Only function code 0x03 is implemented in this release.
  const uint8_t reqBase[6]={slave_, 0x03, (uint8_t)(addr>>8),(uint8_t)addr,0x00,0x01};
  uint8_t req[8];
  memcpy(req,reqBase,6);
  uint16_t crc=crc16(req,6);
  req[6]=(uint8_t)crc;req[7]=(uint8_t)(crc>>8);
  while(bus_.available()) bus_.read(); // discard stale responses
  digitalWrite(RS485_DE_PIN, HIGH);
  delayMicroseconds(250); // settle isolator / TX enable
  const size_t sent=bus_.write(req,sizeof(req));
  bus_.flush(); // wait for UART shift-register to finish transmission
  delayMicroseconds(250);
  digitalWrite(RS485_DE_PIN, LOW);
  if(sent!=sizeof(req)) {error_="uart_tx_failed"; ++failures_;return false;}

  uint8_t resp[7]; size_t count=0;
  const uint32_t start=millis();
  size_t expected=7;
  while(count<expected && millis()-start<MODBUS_RESPONSE_TIMEOUT_MS) {
    if(bus_.available()) {
      resp[count++]=(uint8_t)bus_.read();
      if(count>=2 && resp[1]==0x83) expected=5; // Modbus exception frame
    } else delay(1);
  }
  if(count!=expected) {error_="timeout_or_frame_length"; ++failures_;return false;}
  if(resp[0]!=slave_) {error_="unexpected_slave"; ++failures_;return false;}
  const uint16_t incoming=(uint16_t)resp[expected-2] | ((uint16_t)resp[expected-1]<<8);
  if(crc16(resp,expected-2)!=incoming) {error_="crc_mismatch_or_nonstandard_protocol";++failures_;return false;}
  if(resp[1]==0x83) {error_=String("modbus_exception_")+String(resp[2]);++failures_;return false;}
  if(resp[1]!=0x03 || resp[2]!=0x02) {error_="unexpected_response";++failures_;return false;}
  value=((uint16_t)resp[3]<<8)|resp[4];
  error_="ok"; ++successes_;
  return true;
}
