#include "VfdService.h"
#include "AppConfig.h"
#include <math.h>
constexpr uint16_t VfdService::REGISTERS_[8];

bool VfdService::fresh(size_t idx) const {
  return sample_[idx].known && (millis()-sample_[idx].stamp < SAMPLE_FRESH_MS);
}
void VfdService::tick(bool paused) {
  if(paused || millis()-lastPoll_<VFD_POLL_INTERVAL_MS) return;
  lastPoll_=millis();
  uint16_t value=0;
  const bool ok=bus_.readRegister(REGISTERS_[cursor_],value);
  if(ok) {sample_[cursor_].raw=value;sample_[cursor_].stamp=millis();sample_[cursor_].known=true;}
  // Leave last result internally for diagnostics, but API never sends stale values.
  cursor_=(cursor_+1)%8;
}
String VfdService::toJson(bool paused) const {
  String o; o.reserve(510);
  bool online=false;
  for(size_t i=0;i<8;++i) if(fresh(i)) online=true;
  o+="{\"online\":";o+=online?"true":"false";
  o+=",\"backup_active\":";o+=paused?"true":"false";
  o+=",\"uptime_ms\":";o+=String(millis());
  auto f=[&](const char* name, size_t idx, double scale) {
    o+=",\"";o+=name;o+="\":";
    if(fresh(idx)) o+=String((double)sample_[idx].raw*scale, idx==0 || idx==1 || idx==3 || idx==6 ? 2 : 0);
    else o+="null";
  };
  f("frequency_hz",0,0.01);
  f("dc_bus_v",1,0.1);
  f("output_v",2,1);
  f("current_a",3,0.01);
  f("rated_hz",6,0.01);
  f("rated_rpm",7,1);
  o+=",\"run_status\":";
  if(fresh(4)) o+=String(sample_[4].raw); else o+="null";
  o+=",\"fault_code\":";
  if(fresh(5)) o+=String(sample_[5].raw); else o+="null";
  o+=",\"estimated_rpm\":";
  if(fresh(0) && fresh(6) && fresh(7) && sample_[6].raw > 0 && sample_[7].raw > 0) {
    const double rpm=(double)sample_[7].raw*(double)sample_[0].raw/(double)sample_[6].raw;
    if(isfinite(rpm) && rpm < 100000.0) o+=String(rpm,0); else o+="null";
  } else o+="null";
  o+=",\"speed_type\":\"estimated_nameplate_linear\"";
  o+=",\"scaling_status\":\"manual_unverified_on_hardware\"}";
  return o;
}
