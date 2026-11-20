#include "WebApp.h"
#include "ParameterIndex.h"
#include "AppConfig.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <math.h>

namespace {
constexpr uint16_t REG_COMMAND_SOURCE = 0xF002;
constexpr uint16_t REG_FREQUENCY_SOURCE = 0xF003;
constexpr uint16_t REG_MAX_FREQUENCY = 0xF00A;
constexpr uint16_t REG_UPPER_FREQUENCY = 0xF00C;
constexpr uint16_t REG_LOWER_FREQUENCY = 0xF00E;
constexpr uint16_t REG_COMMUNICATION_SET = 0x1000;
constexpr uint16_t REG_RUN_COMMAND = 0x2000;
constexpr uint16_t REG_RUN_STATE = 0x3000;
constexpr uint16_t COMMAND_FORWARD = 1;
constexpr uint16_t COMMAND_REVERSE = 2;
constexpr uint16_t COMMAND_DECEL_STOP = 6;

struct ControlConfig {
  uint16_t commandSource{0};
  uint16_t frequencySource{0};
  uint16_t runState{0};
  float maximumHz{0};
  float upperHz{0};
  float lowerHz{0};
};

bool readControlConfig(ModbusRTU& mb, ControlConfig& config) {
  uint16_t maximumRaw=0, upperRaw=0, lowerRaw=0;
  return mb.readRegister(REG_COMMAND_SOURCE,config.commandSource) &&
         mb.readRegister(REG_FREQUENCY_SOURCE,config.frequencySource) &&
         mb.readRegister(REG_MAX_FREQUENCY,maximumRaw) &&
         mb.readRegister(REG_UPPER_FREQUENCY,upperRaw) &&
         mb.readRegister(REG_LOWER_FREQUENCY,lowerRaw) &&
         mb.readRegister(REG_RUN_STATE,config.runState) &&
         ((config.maximumHz=maximumRaw*0.01f),
          (config.upperHz=upperRaw*0.01f),
          (config.lowerHz=lowerRaw*0.01f), true);
}

String hzJson(float hz) { return String(hz,2); }
}

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
bool WebApp::controlArmed() {
  if(controlArmed_ && (int32_t)(controlArmedUntilMs_-millis())<=0) disarmControl();
  return controlArmed_;
}
void WebApp::disarmControl() {controlArmed_=false;controlArmedUntilMs_=0;}

void WebApp::initRoutes() {
  server_.on("/",HTTP_GET,[&](){serveFile("/index.html","text/html; charset=utf-8");});
  server_.on("/css/style.css",HTTP_GET,[&](){serveFile("/css/style.css","text/css; charset=utf-8");});
  server_.on("/css/control.css",HTTP_GET,[&](){serveFile("/css/control.css","text/css; charset=utf-8");});
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
    s+=",\"capabilities\":[\"backup\",\"safe_restore_rw\",\"device_name\",\"serial_settings\",\"motor_control\",\"frequency_control\"]}";
    sendJSON(200,s);
  });
  server_.on("/api/control/status",HTTP_GET,[&](){
    if(backup_.active()) {sendJSON(409,"{\"error\":\"backup_running\"}");return;}
    ControlConfig config;
    if(!readControlConfig(mb_,config)) {sendJSON(502,"{\"error\":\"control_preflight_read_failed\",\"detail\":\""+jsonEscape(mb_.lastError())+"\"}");return;}
    const bool armed=controlArmed();
    const uint32_t remaining=armed?(controlArmedUntilMs_-millis())/1000UL:0;
    String s="{\"armed\":"+String(armed?"true":"false")+",\"armed_remaining_seconds\":"+String(remaining);
    s+=",\"command_source\":"+String(config.commandSource)+",\"frequency_source\":"+String(config.frequencySource);
    s+=",\"command_source_ready\":"+String(config.commandSource==2?"true":"false");
    s+=",\"frequency_source_ready\":"+String(config.frequencySource==9?"true":"false");
    s+=",\"run_state\":"+String(config.runState)+",\"maximum_hz\":"+hzJson(config.maximumHz);
    s+=",\"upper_hz\":"+hzJson(config.upperHz)+",\"lower_hz\":"+hzJson(config.lowerHz)+"}";
    sendJSON(200,s);
  });
  server_.on("/api/control/arm",HTTP_POST,[&](){
    if(backup_.active()) {sendJSON(409,"{\"error\":\"backup_running\"}");return;}
    if(server_.arg("confirm")!="ENABLE_REMOTE_CONTROL") {sendJSON(412,"{\"error\":\"confirmation_required\"}");return;}
    ControlConfig config;
    if(!readControlConfig(mb_,config)) {sendJSON(502,"{\"error\":\"control_preflight_read_failed\"}");return;}
    if(config.commandSource!=2||config.frequencySource!=9) {
      sendJSON(409,"{\"error\":\"communication_control_not_configured\",\"required\":\"P0-02=2 and P0-03=9\",\"command_source\":"+String(config.commandSource)+",\"frequency_source\":"+String(config.frequencySource)+"}");return;
    }
    controlArmed_=true;controlArmedUntilMs_=millis()+CONTROL_ARM_DURATION_MS;
    sendJSON(200,"{\"success\":true,\"armed\":true,\"expires_in_seconds\":600}");
  });
  server_.on("/api/control/disarm",HTTP_POST,[&](){
    disarmControl();sendJSON(200,"{\"success\":true,\"armed\":false}");
  });
  server_.on("/api/control/run",HTTP_POST,[&](){
    if(backup_.active()) {sendJSON(409,"{\"error\":\"backup_running\"}");return;}
    if(!controlArmed()) {sendJSON(423,"{\"error\":\"remote_control_not_armed\"}");return;}
    if(server_.arg("confirm")!="START_MOTOR") {sendJSON(412,"{\"error\":\"start_confirmation_required\"}");return;}
    const String direction=server_.arg("direction");
    const uint16_t command=direction=="forward"?COMMAND_FORWARD:direction=="reverse"?COMMAND_REVERSE:0;
    if(command==0) {sendJSON(422,"{\"error\":\"invalid_direction\"}");return;}
    ControlConfig config;
    if(!readControlConfig(mb_,config)) {sendJSON(502,"{\"error\":\"control_preflight_read_failed\"}");return;}
    if(config.commandSource!=2||config.frequencySource!=9) {disarmControl();sendJSON(409,"{\"error\":\"communication_control_not_configured\"}");return;}
    if((config.runState==1||config.runState==2)&&config.runState!=command) {
      sendJSON(409,"{\"error\":\"stop_required_before_direction_change\"}");return;
    }
    if(config.runState!=3&&config.runState!=command) {sendJSON(409,"{\"error\":\"vfd_stop_not_confirmed\"}");return;}
    if(!mb_.writeRegister(REG_RUN_COMMAND,command)) {sendJSON(502,"{\"error\":\"run_command_failed\",\"detail\":\""+jsonEscape(mb_.lastError())+"\"}");return;}
    sendJSON(200,"{\"success\":true,\"command\":\""+direction+"\",\"verification\":\"monitor_register_0x3000\"}");
  });
  server_.on("/api/control/stop",HTTP_POST,[&](){
    // STOP keeps bus priority even after the arm window expires.
    if(backup_.active()) backup_.cancel();
    uint16_t commandSource=0;
    if(!mb_.readRegister(REG_COMMAND_SOURCE,commandSource)) {sendJSON(502,"{\"error\":\"command_source_read_failed\"}");return;}
    if(commandSource!=2) {disarmControl();sendJSON(409,"{\"error\":\"communication_command_not_configured\"}");return;}
    if(!mb_.writeRegister(REG_RUN_COMMAND,COMMAND_DECEL_STOP)) {sendJSON(502,"{\"error\":\"stop_command_failed\",\"detail\":\""+jsonEscape(mb_.lastError())+"\"}");return;}
    sendJSON(200,"{\"success\":true,\"command\":\"deceleration_stop\",\"verification\":\"monitor_register_0x3000\"}");
  });
  server_.on("/api/control/frequency",HTTP_POST,[&](){
    if(backup_.active()) {sendJSON(409,"{\"error\":\"backup_running\"}");return;}
    if(!controlArmed()) {sendJSON(423,"{\"error\":\"remote_control_not_armed\"}");return;}
    const String hzText=server_.arg("hz");char* end=nullptr;const float requested=strtof(hzText.c_str(),&end);
    if(hzText.length()<1||!end||*end!='\0'||!isfinite(requested)) {sendJSON(422,"{\"error\":\"invalid_frequency\"}");return;}
    ControlConfig config;
    if(!readControlConfig(mb_,config)) {sendJSON(502,"{\"error\":\"control_preflight_read_failed\"}");return;}
    if(config.commandSource!=2||config.frequencySource!=9) {disarmControl();sendJSON(409,"{\"error\":\"communication_control_not_configured\"}");return;}
    const float allowedUpper=min(config.maximumHz,config.upperHz);
    if(config.maximumHz<=0||allowedUpper<config.lowerHz||requested<config.lowerHz||requested>allowedUpper) {
      sendJSON(422,"{\"error\":\"frequency_out_of_range\",\"minimum_hz\":"+hzJson(config.lowerHz)+",\"maximum_hz\":"+hzJson(allowedUpper)+"}");return;
    }
    const uint16_t raw=(uint16_t)lroundf((requested/config.maximumHz)*10000.0f);
    if(raw>10000||!mb_.writeRegister(REG_COMMUNICATION_SET,raw)) {
      sendJSON(502,"{\"error\":\"frequency_command_failed\",\"detail\":\""+jsonEscape(mb_.lastError())+"\"}");return;
    }
    sendJSON(200,"{\"success\":true,\"requested_hz\":"+hzJson(requested)+",\"communication_raw\":"+String(raw)+",\"maximum_frequency_hz\":"+hzJson(config.maximumHz)+"}");
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
    uint16_t runState=0;
    if(!mb_.readRegister(REG_RUN_STATE,runState)||runState!=3) {sendJSON(409,"{\"error\":\"vfd_stop_required_for_backup\"}");return;}
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
