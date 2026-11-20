#include "WebApp.h"
#include "ParameterIndex.h"
#include "AppConfig.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>

static String jsonEscape(const String& value) {
  String out;out.reserve(value.length()+8);
  for(size_t i=0;i<value.length();++i) {
    const char c=value.charAt(i);
    if(c=='"'||c=='\\') {out+='\\';out+=c;}
    else if((uint8_t)c>=32) out+=c;
  }
  return out;
}

static String deviceId() {
  String suffix=String((uint32_t)(ESP.getEfuseMac()&0xFFFFFFFFULL),HEX);suffix.toUpperCase();
  while(suffix.length()<8) suffix="0"+suffix;
  return "ESP32-"+suffix;
}

void WebApp::sendJSON(int status,const String& json) {
  server_.sendHeader("Cache-Control","no-store, max-age=0");
  server_.sendHeader("X-Content-Type-Options","nosniff");
  server_.send(status,"application/json; charset=utf-8",json);
}
void WebApp::serveFile(const char* path,const char* mime) {
  File f=LittleFS.open(path,"r");
  if(!f) {server_.send(404,"text/plain","Missing filesystem asset; run Upload Filesystem Image in PlatformIO.");return;}
  server_.sendHeader("Cache-Control","no-store");
  server_.streamFile(f,mime);f.close();
}
void WebApp::begin() {initRoutes();server_.begin();}
void WebApp::tick() {server_.handleClient();}

void WebApp::initRoutes() {
  server_.on("/",HTTP_GET,[&](){serveFile("/index.html","text/html; charset=utf-8");});
  server_.on("/css/style.css",HTTP_GET,[&](){serveFile("/css/style.css","text/css; charset=utf-8");});
  server_.on("/js/app.js",HTTP_GET,[&](){serveFile("/js/app.js","application/javascript; charset=utf-8");});
  server_.on("/api/library",HTTP_GET,[&](){serveFile("/library/ski780_parameters.json","application/json; charset=utf-8");});
  server_.on("/api/monitor",HTTP_GET,[&](){sendJSON(200,vfd_.toJson(backup_.active()));});
  server_.on("/api/status",HTTP_GET,[&](){
    Preferences devicePref;devicePref.begin("sako780",true);
    const String deviceName=devicePref.getString("device_name","SAKO Service Tool");devicePref.end();
    String s="{\"product_id\":\""+String(PRODUCT_ID)+"\",\"api_version\":\""+String(API_VERSION)+"\",";
    s+="\"firmware_version\":\""+String(FW_VERSION)+"\",\"firmware\":\"SAKO SKI780 "+String(FW_VERSION)+"\",";
    s+="\"device_id\":\""+deviceId()+"\",\"device_name\":\""+jsonEscape(deviceName)+"\",";
    s+="\"hardware\":\"Waveshare ESP32-S3-RS485-CAN\",";
    s+="\"slave\":"+String(mb_.slave())+",\"baud\":"+String(mb_.baud());
    const bool staOnline = WiFi.status() == WL_CONNECTED;
    const bool apOnline = (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA);
    const String netMode = staOnline ? (apOnline ? "AP+STA" : "STA") : "AP";
    const String netIp = staOnline ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    const String netSsid = staOnline ? WiFi.SSID() : String(AP_SSID);
    s+=",\"serial_format\":\""+mb_.format()+"\"";
    s+=",\"network_mode\":\""+netMode+"\",\"network_ip\":\""+netIp+"\"";
    // The configured SSID is never reflected blindly into JSON.
    String encodedSsid;
    for(size_t i=0; i<netSsid.length(); ++i) {
      const char ch=netSsid.charAt(i);
      if(ch=='\\' || ch=='\"') encodedSsid+='\\';
      if((unsigned char)ch < 0x20) continue;
      encodedSsid+=ch;
    }
    s+=",\"network_ssid\":\""+encodedSsid+"\"";
    s+=",\"ap_ip\":\""+(apOnline ? WiFi.softAPIP().toString() : "")+"\"";
    s+=",\"ap_ssid\":\""+(apOnline ? String(AP_SSID) : "")+"\",\"successful_reads\":"+String(mb_.successfulReads());
    s+=",\"failed_reads\":"+String(mb_.failedReads())+",\"successful_writes\":"+String(mb_.successfulWrites());
    s+=",\"failed_writes\":"+String(mb_.failedWrites())+",\"last_modbus_error\":\""+jsonEscape(mb_.lastError())+"\"";
    s+=",\"capabilities\":[\"backup\",\"safe_restore_rw\",\"device_name\",\"serial_settings\"]}";
    sendJSON(200,s);
  });
  server_.on("/api/parameter",HTTP_GET,[&](){
    if(backup_.active()) {sendJSON(409,"{\"error\":\"backup_busy\"}");return;}
    if(!server_.hasArg("code")) {sendJSON(400,"{\"error\":\"missing_code\"}");return;}
    String code=server_.arg("code");
    const auto* p=findParameter(code);
    if(!p) {sendJSON(404,"{\"error\":\"unknown_parameter_or_address\"}");return;}
    if(!isReadable(*p)) {sendJSON(403,"{\"error\":\"not_confirmed_readable\"}");return;}
    uint16_t raw=0;
    if(!mb_.readRegister(p->address,raw)) {sendJSON(502,"{\"error\":\"vfd_read_failed\"}");return;}
    sendJSON(200,"{\"code\":\""+code+"\",\"raw_value\":"+String(raw)+"}");
  });
  server_.on("/api/backup/status",HTTP_GET,[&](){sendJSON(200,backup_.statusJson());});
  server_.on("/api/backup/start",HTTP_POST,[&](){
    if(backup_.active()) {sendJSON(409,"{\"error\":\"backup_running\"}");return;}
    String clientTime=server_.arg("time");
    if(!backup_.start(clientTime)) {sendJSON(507,"{\"error\":\"storage_unavailable\"}");return;}
    sendJSON(202,backup_.statusJson());
  });
  server_.on("/api/backup/cancel",HTTP_POST,[&](){backup_.cancel();sendJSON(200,backup_.statusJson());});
  server_.on("/api/backup/download",HTTP_GET,[&](){
    if(backup_.active()) {sendJSON(409,"{\"error\":\"backup_in_progress\"}");return;}
    if(!backup_.ready()) {sendJSON(404,"{\"error\":\"no_completed_backup\"}");return;}
    server_.sendHeader("Content-Disposition","attachment; filename=SAKO_SKI780_backup.json");
    serveFile(BackupService::OUTPUT_PATH,"application/json");
  });
  server_.on("/api/settings",HTTP_POST,[&](){
    if(backup_.active()) {sendJSON(409,"{\"error\":\"backup_running\"}");return;}
    const String baudS=server_.arg("baud"), slaveS=server_.arg("slave"), fmt=server_.arg("format");
    const uint32_t baud=(uint32_t)baudS.toInt();const int slave=slaveS.toInt();
    const bool validBaud=baud==300||baud==600||baud==1200||baud==2400||baud==4800||baud==9600||baud==19200||baud==38400||baud==57600||baud==115200;
    const bool validFormat=fmt=="8N2"||fmt=="8N1"||fmt=="8E1"||fmt=="8O1";
    if(!validBaud||slave<1||slave>247||!validFormat) {sendJSON(422,"{\"error\":\"invalid_serial_settings\"}");return;}
    Preferences pref;pref.begin("sako780",false);
    pref.putUInt("baud",baud);pref.putUChar("slave",(uint8_t)slave);pref.putString("format",fmt);pref.end();
    mb_.begin(baud,fmt,(uint8_t)slave);
    sendJSON(200,"{\"success\":true,\"note\":\"ESP32_port_updated_only_no_VFD_settings_written\"}");
  });
  server_.on("/api/device",HTTP_POST,[&](){
    String name=server_.arg("name");name.trim();
    if(name.length()<1||name.length()>96) {sendJSON(422,"{\"error\":\"invalid_device_name\"}");return;}
    String safe;
    for(size_t i=0;i<name.length();++i) {
      const char c=name.charAt(i);
      if((uint8_t)c>=32&&c!='"'&&c!='\\') safe+=c;
    }
    if(safe.length()<1) {sendJSON(422,"{\"error\":\"invalid_device_name\"}");return;}
    Preferences pref;pref.begin("sako780",false);pref.putString("device_name",safe);pref.end();
    sendJSON(200,"{\"success\":true,\"device_name\":\""+jsonEscape(safe)+"\"}");
  });
  server_.on("/api/restore/parameter",HTTP_POST,[&](){
    if(backup_.active()) {sendJSON(409,"{\"error\":\"backup_running\"}");return;}
    if(server_.arg("confirm")!="RESTORE_RW") {sendJSON(412,"{\"error\":\"confirmation_required\"}");return;}
    const String code=server_.arg("code"),rawText=server_.arg("raw_value");
    const auto* parameter=findParameter(code);
    if(!parameter||parameter->access!=ACCESS_RW||code.startsWith("REG_")) {
      sendJSON(403,"{\"error\":\"not_trusted_rw_parameter\"}");return;
    }
    if(rawText.length()<1||rawText.length()>5) {sendJSON(422,"{\"error\":\"invalid_raw_value\"}");return;}
    char* end=nullptr;const unsigned long parsed=strtoul(rawText.c_str(),&end,10);
    if(!end||*end!='\0'||parsed>65535UL) {sendJSON(422,"{\"error\":\"invalid_raw_value\"}");return;}
    uint16_t runState=0;
    if(!mb_.readRegister(0x3000,runState)||runState!=3) {
      sendJSON(409,"{\"error\":\"vfd_stop_not_confirmed\"}");return;
    }
    const uint16_t requested=(uint16_t)parsed;
    uint16_t current=0;
    if(!mb_.readRegister(parameter->address,current)) {
      sendJSON(502,"{\"error\":\"current_value_read_failed\"}");return;
    }
    if(current==requested) {
      sendJSON(200,"{\"success\":true,\"changed\":false,\"code\":\""+code+"\",\"raw_value\":"+String(requested)+",\"readback\":"+String(current)+"}");return;
    }
    if(!mb_.writeRegister(parameter->address,requested)) {
      sendJSON(502,"{\"error\":\"vfd_write_failed\",\"detail\":\""+jsonEscape(mb_.lastError())+"\"}");return;
    }
    uint16_t readback=0;
    if(!mb_.readRegister(parameter->address,readback)||readback!=requested) {
      sendJSON(502,"{\"error\":\"readback_verification_failed\"}");return;
    }
    sendJSON(200,"{\"success\":true,\"changed\":true,\"code\":\""+code+"\",\"raw_value\":"+String(requested)+",\"readback\":"+String(readback)+"}");
  });
  server_.onNotFound([&](){server_.send(404,"text/plain","Not Found");});
}
