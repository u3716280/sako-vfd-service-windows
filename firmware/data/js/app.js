'use strict';
/* SAKO SKI780 v0.1: browser UI. This release NEVER writes Modbus registers. */
const $ = id => document.getElementById(id);
const state = { page:'dashboard', library:null, entries:[],filtered:[],pageNo:0,perPage:22,selected:null,settingsLoaded:false,profiles:[],backup:null };
const pages = {dashboard:'Monitoring Dashboard',parameters:'Parameter Manager',library:'Parameter Library',backup:'JSON Backup',settings:'Settings'};
const DEFAULT_DISPLAY='—';
function txt(id,value){const el=$(id);if(el)el.textContent=value==null?DEFAULT_DISPLAY:String(value);}
function htmlEscape(v){return String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}
let toastTimer=0;
function toast(msg,error=false){const t=$('toast');t.textContent=msg;t.className='toast show'+(error?' error':'');clearTimeout(toastTimer);toastTimer=setTimeout(()=>t.className='toast',3800);}
async function api(url,opts){const res=await fetch(url,{cache:'no-store',...opts});let data;try{data=await res.json();}catch{throw new Error('Invalid server response');}if(!res.ok)throw new Error(data?.error||`HTTP ${res.status}`);return data;}
function switchPage(page){state.page=page;for(const k of Object.keys(pages))$('page-'+k).classList.toggle('active',k===page);document.querySelectorAll('[data-nav]').forEach(el=>el.classList.toggle('active',el.dataset.nav===page));txt('page-title',pages[page]);if(page==='parameters'||page==='library')loadLibrary();if(page==='settings')loadDeviceStatus();if(page==='backup')loadBackupStatus();}
document.querySelectorAll('[data-nav]').forEach(b=>b.addEventListener('click',()=>switchPage(b.dataset.nav)));
function formatMetric(v,digits=1){return Number.isFinite(v)?Number(v).toLocaleString('en-US',{minimumFractionDigits:digits,maximumFractionDigits:digits}):DEFAULT_DISPLAY;}
function updateDashboard(m){
 const online=!!m.online;
 const p=$('connection-pill');p.className='pill '+(online?'':'off');txt('connection-text',online?'RS-485 Online':'RS-485 Offline');
 const banner=$('warn-banner');banner.className='banner '+(online?'good':'warning');
 banner.textContent=m.backup_active?'Backup in progress — live polling paused; data may be stale.':online?'Connected · Read-only monitoring · Not a motor safety measurement':'Not receiving fresh data from SAKO VFD. Check power, A/B wiring, Slave ID, serial settings and Standard Modbus.';
 txt('run-status',m.run_status===1||m.run_status===2?'RUN':m.run_status===3?'STOP':'—');
 txt('direction',m.run_status===1?'FORWARD':m.run_status===2?'REVERSE':m.run_status===3?'—':'—');
 const d=$('run-dot');d.textContent=m.run_status===1||m.run_status===2?'●':'◯';d.className='status-icon '+((m.run_status===1||m.run_status===2)?'running':(m.run_status===3?'stopped':''));
 txt('m-voltage',formatMetric(m.output_v,0));txt('m-current',formatMetric(m.current_a,2));
 txt('m-frequency',formatMetric(m.frequency_hz,2));txt('m-rpm',formatMetric(m.estimated_rpm,0));
 txt('m-bus',formatMetric(m.dc_bus_v,1));
 txt('m-fault',m.fault_code!=null?'0x'+Number(m.fault_code).toString(16).toUpperCase().padStart(4,'0'):DEFAULT_DISPLAY);
 txt('fault-label',m.fault_code===0?'No fault':m.fault_code!=null?'Active/Reported':'');
 txt('sampling',m.backup_active?'Live polling paused during backup':'Polling one Modbus register per ~105ms');
 txt('data-age',online?'Only measurements younger than 3.3s are displayed':'Values hidden when stale');
}
async function updateMonitor(){try{updateDashboard(await api('/api/monitor'));}catch{updateDashboard({online:false});}}
async function loadDeviceStatus(){try{const s=await api('/api/status');txt('status-slave',s.slave);txt('status-baud',`${s.baud} · ${s.serial_format}`);txt('wifi-mode',s.network_mode||'AP');txt('wifi-ssid',s.network_ssid||s.ap_ssid);txt('wifi-ip','http://'+(s.network_ip||s.ap_ip));txt('firmware-version',s.firmware);txt('read-ok',s.successful_reads);txt('read-bad',s.failed_reads);txt('modbus-error','Last Modbus response: '+s.last_modbus_error);if(!state.settingsLoaded){$('set-slave').value=s.slave;$('set-baud').value=s.baud;$('set-format').value=s.serial_format;state.settingsLoaded=true;}}catch(e){if(state.page==='settings')toast(e.message,true);}}
async function loadLibrary(){
 if(state.library)return;
 try{
  const data=await api('/api/library');if(!Array.isArray(data.parameters))throw new Error('Invalid parameter library');
  state.library=data;state.entries=data.parameters;
  txt('lib-count',`${data.parameters.length} entries`);txt('lib-version',data.library_id||data.schema_version||'v1');
  const groups=Array.from(new Set(data.parameters.map(p=>p.group))).sort();
  $('param-group').innerHTML='<option value="">All Groups</option>'+groups.map(g=>`<option value="${htmlEscape(g)}">${htmlEscape(g)}</option>`).join('');
  const counts=data.parameters.reduce((a,p)=>(a[p.access]=(a[p.access]||0)+1,a),{});
  $('access-counters').innerHTML=Object.entries(counts).map(([k,v])=>`<span>${htmlEscape(k)}: <strong>${v}</strong></span>`).join('');
  filterParams();
 }catch(e){toast('Library unavailable: '+e.message,true);txt('lib-count','Unavailable');}
}
function filterParams(){if(!state.library)return;const q=$('param-search').value.trim().toLowerCase(),g=$('param-group').value,a=$('param-access').value;state.filtered=state.entries.filter(p=>(!g||p.group===g)&&(!a||p.access===a)&&(!q||[p.code,p.name,p.address,p.description].some(x=>String(x??'').toLowerCase().includes(q))));state.pageNo=0;renderParams();}
function renderParams(){const rows=state.filtered,start=state.pageNo*state.perPage;const shown=rows.slice(start,start+state.perPage);
 $('param-tbody').innerHTML=shown.map(p=>`<tr><td><strong>${htmlEscape(p.code)}</strong></td><td>${htmlEscape(p.name)}</td><td><span class="access ${String(p.access).toLowerCase()}">${htmlEscape(p.access)}</span></td><td>${htmlEscape(p.address||'—')}</td><td><button class="mini-btn" type="button" data-detail="${htmlEscape(p.code)}">Details</button></td></tr>`).join('')||'<tr><td colspan="5">No matching parameters.</td></tr>';
 txt('param-pageinfo',`${rows.length?start+1:0}–${Math.min(start+state.perPage,rows.length)} of ${rows.length}`);
 $('prev-page').disabled=state.pageNo===0;$('next-page').disabled=(state.pageNo+1)*state.perPage>=rows.length;
}
$('param-search').addEventListener('input',filterParams);$('param-group').addEventListener('change',filterParams);$('param-access').addEventListener('change',filterParams);
$('prev-page').addEventListener('click',()=>{state.pageNo=Math.max(0,state.pageNo-1);renderParams();});
$('next-page').addEventListener('click',()=>{state.pageNo++;renderParams();});
$('param-tbody').addEventListener('click',e=>{const btn=e.target.closest('[data-detail]');if(!btn)return;state.selected=state.entries.find(p=>p.code===btn.dataset.detail);renderParameterDetail();});
function field(label,value){return `<div class="info"><span class="label">${htmlEscape(label)}</span><strong>${htmlEscape(value==null?'Not specified':value)}</strong></div>`;}
function renderParameterDetail(){const p=state.selected;if(!p)return;
 const details=$('param-detail');const readable=p.access==='RO'||p.access==='RW';
 details.innerHTML=`<div class="panel-header"><div><h2>${htmlEscape(p.code)} · ${htmlEscape(p.name)}</h2><p>${htmlEscape(p.description||'No description available')}</p></div><span class="access ${String(p.access).toLowerCase()}">${htmlEscape(p.access)}</span></div><div class="detail-grid">
 ${field('Address',p.address)}${field('Default',p.default_value??p.default_value_note)}${field('Minimum',p.min_value)}${field('Maximum',p.max_value)}${field('Unit',p.unit)}${field('Range / Enumeration',p.setting_range_text)}${field('Write requires VFD stopped',p.write_conditions?.requires_stop===true?'YES':p.write_conditions?.requires_stop===false?'Not marked':'Unconfirmed')}${field('Source PDF page',p.source?.pdf_page)}
 </div><p class="subdued">Source: ${htmlEscape(p.source?.document||'Unknown')} · Hardware validation: ${p.validated_on_hardware?'Yes':'Not performed'}</p><div class="flex-wrap">${readable?`<button class="btn primary" id="read-selected">Read Current Raw Value</button>`:'<span class="warning-note">This parameter is not confirmed readable; read is disabled.</span>'}<strong id="live-param-value" class="subdued">—</strong></div>`;
 if(readable)$('read-selected').addEventListener('click',()=>readSelected(p));
}
async function readSelected(p){const out=$('live-param-value');out.textContent='Reading…';try{const d=await api('/api/parameter?code='+encodeURIComponent(p.code));out.textContent=`Raw: ${d.raw_value}`+(typeof p.display_resolution==='number'?` · Display estimate: ${(d.raw_value*p.display_resolution).toString()} ${p.unit||''} (verify scale)`:' · Scaling not confirmed');}catch(e){out.textContent='Read failed: '+e.message;toast(e.message,true);}}
function setBackupState(s){state.backup=s;const total=s.total||0;txt('backup-message',({idle:'Not started',reading:'Reading parameters via RS-485',completed:'Backup finished — review failures',cancelled:'Cancelled',storage_error:'Storage error'})[s.stage]||s.stage);txt('backup-progress-label',`${s.processed} / ${total}`);$('backup-progress').style.width=(total?100*s.processed/total:0)+'%';txt('b-ok',s.success);txt('b-fail',s.failed);txt('b-skipped',s.skipped);$('backup-start').disabled=!!s.active;$('backup-cancel').disabled=!s.active;const dl=$('backup-download');dl.classList.toggle('disabled',!s.ready);dl.setAttribute('aria-disabled',String(!s.ready));}
async function loadBackupStatus(){try{setBackupState(await api('/api/backup/status'));}catch(e){toast(e.message,true);}}
$('backup-start').addEventListener('click',async()=>{if(!confirm('Read all supported RO/RW registers from the VFD? This can take time, and the result may be partial.'))return;try{const ts=new Date().toISOString();setBackupState(await api('/api/backup/start?time='+encodeURIComponent(ts),{method:'POST'}));toast('Backup started. Keep ESP32 powered and connected.');}catch(e){toast(e.message,true);}});
$('backup-cancel').addEventListener('click',async()=>{try{setBackupState(await api('/api/backup/cancel',{method:'POST'}));}catch(e){toast(e.message,true);}});
async function pollBackup(){if(state.page!=='backup'&&!state.backup?.active)return;try{const s=await api('/api/backup/status');const was=state.backup?.active;setBackupState(s);if(was&&!s.active&&s.ready)toast(s.failed?'Backup complete with read failures: check JSON.':'Backup complete. Download JSON.');}catch(e){}}
function loadProfiles(){try{const raw=localStorage.getItem('sako_ski780_profiles_v1');state.profiles=raw?JSON.parse(raw):[];if(!Array.isArray(state.profiles))state.profiles=[];}catch{state.profiles=[];}renderProfiles();}
function saveProfiles(){try{localStorage.setItem('sako_ski780_profiles_v1',JSON.stringify(state.profiles));return true;}catch(e){toast('Browser storage full or disabled; export JSON instead.',true);return false;}}
function saveJSON(data,filename){const a=document.createElement('a'),u=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)],{type:'application/json'}));a.href=u;a.download=filename;a.click();setTimeout(()=>URL.revokeObjectURL(u),2000);}
function renderProfiles(){const box=$('profile-list');if(!state.profiles.length){box.innerHTML='<p class="subdued">No profiles imported.</p>';return;}
 box.innerHTML=state.profiles.map((p,i)=>`<div class="profile-row"><div><strong>${htmlEscape(p.name)}</strong><small>${htmlEscape(p.created)} · ${htmlEscape(p.entries)} entries · Stored locally in this browser</small></div><div class="flex-wrap"><button class="btn outline" data-profile-export="${i}">Export</button><button class="btn outline" data-profile-remove="${i}">Delete</button></div></div>`).join('');}
$('profile-import').addEventListener('change',async(e)=>{const file=e.target.files?.[0];if(!file)return;try{if(file.size>1300000)throw new Error('Profile exceeds 1.3MB browser import limit');const data=JSON.parse(await file.text());
 if(data?.type==='local_profile_collection' && Array.isArray(data.profiles)){
  const imported=data.profiles.filter(p=>p&&typeof p.name==='string'&&p.data&&(Array.isArray(p.data.parameters)||typeof p.data.parameters==='object'&&p.data.parameters!==null));
  if(!imported.length)throw new Error('Profile collection is empty or invalid');
  const before=state.profiles.slice();state.profiles.push(...imported);
  if(saveProfiles()){renderProfiles();toast(`${imported.length} profiles imported`);}else state.profiles=before;
 }else{
  if(!data || !(Array.isArray(data.parameters)||typeof data.parameters==='object'&&data.parameters!==null))throw new Error('JSON must contain parameters array/object');
  const name=prompt('Name this profile:',file.name.replace(/\.json$/i,''));if(!name)return;
  const entries=Array.isArray(data.parameters)?data.parameters.length:Object.keys(data.parameters).length;
  state.profiles.push({name:name.substring(0,90),created:new Date().toISOString(),entries,data});
  if(saveProfiles()){renderProfiles();toast('Profile imported to this browser');}else state.profiles.pop();
 }
 }catch(err){toast('Import failed: '+err.message,true);}finally{e.target.value='';}});
$('profile-list').addEventListener('click',e=>{const exp=e.target.closest('[data-profile-export]'),del=e.target.closest('[data-profile-remove]');if(exp){const p=state.profiles[+exp.dataset.profileExport];if(p)saveJSON(p.data,p.name.replace(/[^a-z0-9_-]/gi,'_')+'.json');}if(del){const i=+del.dataset.profileRemove;if(confirm(`Delete profile "${state.profiles[i]?.name}" from this browser?`)){state.profiles.splice(i,1);saveProfiles();renderProfiles();}}});
$('profile-export-all').addEventListener('click',()=>{if(!state.profiles.length){toast('There are no local profiles to export',true);return;}saveJSON({schema_version:1,type:'local_profile_collection',profiles:state.profiles},'SAKO_Profiles.json');});
$('settings-form').addEventListener('submit',async e=>{e.preventDefault();try{const body=new URLSearchParams({slave:$('set-slave').value,baud:$('set-baud').value,format:$('set-format').value});await api('/api/settings',{method:'POST',body});state.settingsLoaded=false;txt('settings-result','ESP32 serial settings saved. No changes made to the inverter.');toast('ESP32 Modbus port updated');loadDeviceStatus();}catch(e){toast(e.message,true);}});
loadProfiles();loadDeviceStatus();updateMonitor();setInterval(updateMonitor,1000);setInterval(pollBackup,800);setInterval(()=>{if(state.page==='settings'||state.page==='dashboard')loadDeviceStatus();},5000);
