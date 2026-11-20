#!/usr/bin/env python3
"""Host-only checks. Does NOT build, flash or test ESP32/VFD hardware."""
from pathlib import Path
from collections import Counter
import json,re
root=Path(__file__).resolve().parents[1]
lib=json.loads((root/'data/library/ski780_parameters.json').read_text())
params=lib['parameters']
assert len(params)==448,(len(params),'expected 448')
counts=Counter(p['access'] for p in params)
assert counts==Counter({'RW':309,'RO':126,'WO':8,'Restricted':2,'Unknown':3}),counts
codes=[p['code'] for p in params]
assert len(codes)==len(set(codes)),'duplicate parameter code'
idx=(root/'include/ParameterIndex.h').read_text()
entries=re.findall(r'^  \{"([^"]+)", 0x([0-9A-F]{4}), (\d)\},$',idx,re.M)
assert len(entries)==445,len(entries)
for code,addr,access in entries:
    p=next(p for p in params if p['code']==code)
    assert p['address'].upper()==('0X'+addr).replace('0X','0x').upper(),(code,addr,p['address'])
    assert {'RO':0,'RW':1,'WO':2,'Restricted':3,'Unknown':4}[p['access']]==int(access)
assert (root/'data/library/ski780_parameters.json').stat().st_size<800000
cpp='\n'.join(x.read_text() for x in (root/'src').glob('*.cpp'))
modbus=(root/'src/ModbusRTU.cpp').read_text()
web=(root/'src/WebApp.cpp').read_text()
assert '0x03' in modbus and '0x06' in modbus
assert 'not_trusted_rw_parameter' in web
assert 'vfd_stop_not_confirmed' in web
assert 'readback_verification_failed' in web
assert 'parameter->access!=ACCESS_RW' in web
assert 'REG_' in web, 'command/monitor registers must be excluded from restore'
html=(root/'data/index.html').read_text()
js=(root/'data/js/app.js').read_text()
for target in ['dashboard','parameters','library','backup','settings']:
    assert f'id="page-{target}"' in html
for endpoint in ['/api/monitor','/api/status','/api/library','/api/parameter','/api/backup/start','/api/backup/status']:
    assert endpoint in (root/'src/WebApp.cpp').read_text()
assert '0x7000,0x7002,0x7003,0x7004' in (root/'src/VfdService.cpp').read_text() or '0x7000,0x7002,0x7003,0x7004' in (root/'include/VfdService.h').read_text()
assert 'board_build.filesystem = littlefs' in (root/'platformio.ini').read_text()
assert 'factory' in (root/'partitions.csv').read_text() and 'littlefs' in (root/'partitions.csv').read_text()
for endpoint in ['/api/device','/api/restore/parameter']:
    assert endpoint in web
for endpoint in ['/api/control/status','/api/control/arm','/api/control/disarm','/api/control/run','/api/control/stop','/api/control/frequency']:
    assert endpoint in web
for safety_guard in ['ENABLE_REMOTE_CONTROL','START_MOTOR','stop_required_before_direction_change','vfd_stop_required_for_backup','COMMAND_DECEL_STOP','CONTROL_ARM_DURATION_MS']:
    assert safety_guard in web or safety_guard in (root/'include/AppConfig.h').read_text()
for control_asset in ['id="control-start"','id="control-stop"','id="control-hz"','data-direction="forward"','data-direction="reverse"']:
    assert control_asset in html
assert '/css/control.css' in web and (root/'data/css/control.css').is_file()
assert 'displayParameterValue' in js and 'Value Meaning / Range' in js
assert 'sako-ski780-service-tool' in (root/'include/AppConfig.h').read_text()
print('PASS library 448, trusted index 445, API identity, backup, safe RW restore, guarded motor/frequency control, LittleFS')
