#include "WebApp.h"
#include "ParameterIndex.h"
#include "AppConfig.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>

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
    String s="{\"firmware\":\""+String(FW_VERSION)+"\",\"hardware\":\"Waveshare ESP32-S3-RS485-CAN\",";
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
    s+=",\"failed_reads\":"+String(mb_.failedReads())+",\"last_modbus_error\":\""+mb_.lastError()+"\"}";
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
  server_.onNotFound([&](){server_.send(404,"text/plain","Not Found");});
}
