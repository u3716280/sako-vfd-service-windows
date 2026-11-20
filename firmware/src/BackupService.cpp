#include "BackupService.h"
#include "ParameterIndex.h"
#include <LittleFS.h>

constexpr const char* BackupService::OUTPUT_PATH;

String BackupService::safeJSON(const String& s) {
  String out;out.reserve(s.length()+8);
  for(size_t i=0;i<s.length();++i) {
    const char c=s[i];
    if(c=='"' || c=='\\') {out+='\\';out+=c;}
    else if(c=='\n') out+="\\n";
    else if(c=='\r') out+="\\r";
    else if((uint8_t)c>=32) out+=c;
  }
  return out;
}

bool BackupService::start(const String& browserTime) {
  if(active_) return false;
  if(vals_) vals_.close(); if(errs_) errs_.close();
  LittleFS.remove("/backup_values.tmp");LittleFS.remove("/backup_errors.tmp");
  // Preserve previous good backup until output is ready; the next finish replaces it.
  vals_=LittleFS.open("/backup_values.tmp","w");
  errs_=LittleFS.open("/backup_errors.tmp","w");
  if(!vals_ || !errs_) {
    stage_="storage_error";if(vals_)vals_.close();if(errs_)errs_.close();return false;
  }
  clientTimestamp_=browserTime.substring(0,40);
  next_=processed_=ok_=failed_=skipped_=total_=0;
  haveVal_=haveErr_=false;
  for(size_t i=0;i<PARAMETER_COUNT;++i)
    if(isReadable(PARAMETER_INDEX[i]))++total_;
    else ++skipped_;
  ready_=false;active_=true;stage_="reading";
  return true;
}

void BackupService::tick() {
  if(!active_) return;
  while(next_<PARAMETER_COUNT && !isReadable(PARAMETER_INDEX[next_])) ++next_;
  if(next_>=PARAMETER_COUNT) {finish();return;}
  const auto &p=PARAMETER_INDEX[next_++];
  uint16_t raw=0;
  bool success=bus_.readRegister(p.address,raw);
  if(success) {
    if(haveVal_) vals_.print(',');
    vals_.print("{\"code\":\""); vals_.print(p.code);
    vals_.print("\",\"address\":\"0x"); vals_.print(p.address,HEX);
    vals_.print("\",\"raw_value\":"); vals_.print(raw);
    vals_.print(",\"access\":\""); vals_.print(p.access==ACCESS_RW?"RW":"RO");
    vals_.print("\"}"); haveVal_=true; ++ok_;
  } else {
    if(haveErr_) errs_.print(',');
    errs_.print("{\"code\":\"");errs_.print(p.code);
    errs_.print("\",\"error\":\"");errs_.print(safeJSON(bus_.lastError()));errs_.print("\"}");
    haveErr_=true; ++failed_;
  }
  ++processed_;
}

void BackupService::copyContent(File& from, File& into) {
  from.seek(0);
  uint8_t buffer[256];
  while(from.available()) {
    size_t n=from.read(buffer,sizeof(buffer));
    if(!n) break;
    into.write(buffer,n);
    yield();
  }
}

void BackupService::finish() {
  vals_.flush();errs_.flush(); vals_.close(); errs_.close();
  File sourceValues=LittleFS.open("/backup_values.tmp","r");
  File sourceErrors=LittleFS.open("/backup_errors.tmp","r");
  LittleFS.remove("/backup_next.tmp");
  File result=LittleFS.open("/backup_next.tmp","w");
  if(!sourceValues||!sourceErrors||!result) {
    if(result)result.close();if(sourceValues)sourceValues.close();if(sourceErrors)sourceErrors.close();
    active_=false;ready_=LittleFS.exists(OUTPUT_PATH);stage_="storage_error";return;
  }
  result.print("{\"schema_version\":1,\"type\":\"ski780_modbus_backup\",");
  result.print("\"device\":{\"manufacturer\":\"SAKO\",\"model\":\"SKI780\",\"slave_id\":");
  result.print(bus_.slave());result.print("},");
  result.print("\"created_at_client\":\"");result.print(safeJSON(clientTimestamp_));result.print("\",");
  result.print("\"time_source\":\"browser_clock_unverified\",");
  result.print("\"parameter_library\":\"sako_ski780_library_v1_engineering_draft\",");
  result.print("\"complete\":");result.print(failed_==0?"true":"false");result.print(",");
  result.print("\"counts\":{\"readable_attempted\":");result.print(processed_);
  result.print(",\"read_succeeded\":");result.print(ok_);
  result.print(",\"read_failed\":");result.print(failed_);
  result.print(",\"excluded_non_readable\":");result.print(skipped_);result.print("},");
  result.print("\"parameters\":[");copyContent(sourceValues,result);
  result.print("],\"failed\":[");copyContent(sourceErrors,result);
  result.print("],\"restore_allowed\":true,");
  result.print("\"restore_policy\":\"trusted_firmware_index_rw_only_stopped_readback\"}");
  result.flush();const bool fileOk=result.size()>50;
  result.close();sourceValues.close();sourceErrors.close();
  LittleFS.remove("/backup_values.tmp");LittleFS.remove("/backup_errors.tmp");
  if(fileOk) {
    LittleFS.remove(OUTPUT_PATH);
    ready_=LittleFS.rename("/backup_next.tmp",OUTPUT_PATH);
  } else ready_=false;
  active_=false;stage_=ready_?"completed":"storage_error";
}

void BackupService::cancel() {
  if(!active_)return;
  active_=false;if(vals_)vals_.close();if(errs_)errs_.close();
  LittleFS.remove("/backup_values.tmp");LittleFS.remove("/backup_errors.tmp");
  ready_=false;stage_="cancelled";
}
String BackupService::statusJson() const {
  String s="{\"active\":";s+=active_?"true":"false";
  s+=",\"ready\":";s+=ready_?"true":"false";
  s+=",\"stage\":\""+safeJSON(stage_)+"\"";
  s+=",\"processed\":"+String(processed_);
  s+=",\"total\":"+String(total_);
  s+=",\"success\":"+String(ok_);
  s+=",\"failed\":"+String(failed_);
  s+=",\"skipped\":"+String(skipped_);
  s+="}";return s;
}
