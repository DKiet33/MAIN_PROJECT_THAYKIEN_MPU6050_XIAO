#pragma once
#include <pgmspace.h>

// ════════════════════════════════════════════════════════════════
// html_page.h — Web Dashboard HTML
// ════════════════════════════════════════════════════════════════

static const char HTML_PAGE[] PROGMEM = R"~~~(
<!DOCTYPE html><html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wearable Ingestion</title>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0a0f1a;color:#e2e8f0;font-family:'Inter',sans-serif;padding:14px;font-size:13px}
.header{border-bottom:1px solid #1e293b;padding-bottom:10px;margin-bottom:14px;display:flex;justify-content:space-between;align-items:center}
h1{font-size:14px;color:#38bdf8;font-weight:600;letter-spacing:.04em}
.sub{font-size:11px;color:#64748b;font-family:'JetBrains Mono',monospace}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:8px;margin-bottom:12px}
.box{background:#111827;padding:10px;border:1px solid #1f2937;border-radius:4px}
.lbl{font-size:10px;color:#6b7280;text-transform:uppercase;letter-spacing:.06em}
.val{font-size:15px;font-weight:600;margin-top:4px;color:#f1f5f9;font-family:'JetBrains Mono',monospace}
.state-idle{border-color:#334155;color:#94a3b8}
.state-collecting{background:#0f2a0f;border-color:#22c55e;color:#86efac}
.state-uploading{background:#0f0f2e;border-color:#6366f1;color:#a5b4fc}
.state-ok{background:#064e3b;border-color:#10b981;color:#6ee7b7}
.state-err{background:#7f1d1d;border-color:#ef4444;color:#fca5a5}
.prog-wrap{background:#1e293b;height:6px;margin-top:8px;border-radius:3px;overflow:hidden}
.prog-bar{background:#22c55e;height:6px;transition:width .15s;border-radius:3px}
button{display:block;width:100%;padding:11px;margin-top:12px;background:#0284c7;color:#fff;border:none;
  font-family:'Inter',sans-serif;font-size:13px;font-weight:600;cursor:pointer;border-radius:4px;letter-spacing:.04em}
button:hover{background:#0369a1}
button:disabled{background:#1e293b;color:#475569;cursor:not-allowed}
.logs-row{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:12px}
@media(max-width:600px){.logs-row{grid-template-columns:1fr}}
.log-panel{display:flex;flex-direction:column}
.log-title{font-size:10px;color:#64748b;text-transform:uppercase;letter-spacing:.08em;margin-bottom:5px;font-weight:600}
.log{background:#030712;border:1px solid #1f2937;border-radius:4px;padding:10px;
  height:200px;overflow-y:auto;font-family:'JetBrains Mono',monospace;font-size:11.5px;line-height:1.6;white-space:pre-wrap;word-break:break-all}
.log-collect{color:#86efac}
.log-serial{color:#7dd3fc}
.label-row{display:flex;gap:8px;margin-top:12px}
.lbl-btn{flex:1;padding:9px 4px;background:#111827;color:#94a3b8;border:1px solid #334155;
  border-radius:4px;font-family:'JetBrains Mono',monospace;font-size:13px;font-weight:500;
  cursor:pointer;transition:all .15s}
.lbl-btn:hover{border-color:#475569;color:#e2e8f0}
.lbl-btn.active{background:#0c1f3d;border-color:#38bdf8;color:#38bdf8;font-weight:600}
.lbl-btn:disabled{opacity:.4;cursor:not-allowed}
.state-calibrating{background:#1c1506;border-color:#f59e0b;color:#fbbf24}
.cal-bar-wrap{background:#1e293b;height:4px;margin-top:6px;border-radius:2px;overflow:hidden}
.cal-bar{background:#f59e0b;height:4px;transition:width .2s;border-radius:2px}
.cal-tag{display:inline-block;font-size:10px;padding:2px 8px;border-radius:10px;font-family:'JetBrains Mono',monospace;margin-left:6px}
.cal-ok{background:#064e3b;color:#6ee7b7;border:1px solid #10b981}
.cal-no{background:#1e293b;color:#64748b;border:1px solid #334155}
.auto-row{display:flex;gap:8px;margin-top:12px;align-items:center}
.auto-row input{width:70px;padding:8px;background:#111827;color:#f1f5f9;border:1px solid #334155;
  border-radius:4px;font-family:'JetBrains Mono',monospace;font-size:13px;text-align:center}
.auto-progress{font-size:11px;color:#94a3b8;font-family:'JetBrains Mono',monospace;white-space:nowrap}
.aug-off{background:#1e293b;color:#64748b;border:1px solid #334155;border-radius:4px;padding:8px 14px;
  font-family:'Inter',sans-serif;font-size:12px;font-weight:600;cursor:pointer;white-space:nowrap;margin-top:0}
.aug-off:hover{border-color:#6366f1;color:#a5b4fc}
.aug-on{background:#1e1b4b;color:#a5b4fc;border:2px solid #6366f1;border-radius:4px;padding:8px 14px;
  font-family:'Inter',sans-serif;font-size:12px;font-weight:600;cursor:pointer;white-space:nowrap;margin-top:0}
.jit-off{background:#1e293b;color:#64748b;border:1px solid #334155;border-radius:4px;padding:8px 14px;
  font-family:'Inter',sans-serif;font-size:12px;font-weight:600;cursor:pointer;white-space:nowrap;margin-top:0}
.jit-off:hover{border-color:#10b981;color:#6ee7b7}
.jit-on{background:#052e16;color:#6ee7b7;border:2px solid #10b981;border-radius:4px;padding:8px 14px;
  font-family:'Inter',sans-serif;font-size:12px;font-weight:600;cursor:pointer;white-space:nowrap;margin-top:0}
.manual-box{background:#0d0d1a;border:1px solid #312e81;border-radius:6px;padding:12px;margin-top:12px}
.manual-box .log-title{color:#818cf8}
.manual-file-row{display:flex;gap:8px;align-items:center;margin-top:8px;flex-wrap:wrap}
.manual-file-row input[type=text]{width:80px;padding:6px 8px;background:#111827;color:#f1f5f9;
  border:1px solid #334155;border-radius:4px;font-family:'JetBrains Mono',monospace;font-size:12px}
.manual-hint{font-size:10px;color:#64748b;font-family:'JetBrains Mono',monospace;margin-top:6px}
.aug-cfg-row{display:flex;gap:12px;margin-top:10px;flex-wrap:wrap;align-items:flex-end}
.cfg-item{display:flex;flex-direction:column;gap:3px}
.cfg-item label{font-size:10px;color:#94a3b8;text-transform:uppercase;letter-spacing:.06em;font-weight:600}
.cfg-item input[type=number]{padding:5px 7px;background:#111827;color:#f1f5f9;
  border:1px solid #334155;border-radius:4px;font-family:'JetBrains Mono',monospace;font-size:12px;width:75px}
.cfg-item input[type=number]:focus{outline:none;border-color:#6366f1}
.man-prog-label{font-size:10.5px;color:#94a3b8;font-family:'JetBrains Mono',monospace;
  white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:70%;display:inline-block}
.two-col{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:12px;align-items:start}
@media(max-width:800px){.two-col{grid-template-columns:1fr}}
.collect-box{background:#111827;border:1px solid #1f2937;border-radius:6px;padding:12px}
.collect-box .log-title{color:#38bdf8;margin-bottom:10px}
.label-irow{display:flex;gap:8px;align-items:center;margin-bottom:8px}
.label-irow input[type=text]{flex:1;padding:7px 10px;background:#0a0f1a;color:#f1f5f9;
  border:1px solid #334155;border-radius:4px;font-family:'JetBrains Mono',monospace;
  font-size:13px;font-weight:600;min-width:0}
.label-irow input[type=text]:focus{outline:none;border-color:#38bdf8}
</style>
</head>
<body>
<div class="header">
  <h1>WEARABLE INGESTION</h1>
  <div class="sub" id="ip">--</div>
</div>
<div class="grid">
  <div class="box" id="box-state"><div class="lbl">Trạng thái</div><div class="val" id="st">--</div></div>
  <div class="box"><div class="lbl">Label</div><div class="val" id="lbl">--</div></div>
  <div class="box"><div class="lbl">Tiến độ</div><div class="val" id="prog">0 / 0</div>
    <div class="prog-wrap"><div class="prog-bar" id="bar" style="width:0%"></div></div></div>
  <div class="box"><div class="lbl">Upload gần nhất</div><div class="val" id="last">--</div></div>
</div>
<div class="grid">
  <div class="box"><div class="lbl">aX — Fwd (g)</div><div class="val" id="ax">0.000</div></div>
  <div class="box"><div class="lbl">aY — Left (g)</div><div class="val" id="ay">0.000</div></div>
  <div class="box"><div class="lbl">aZ — Up (g)</div><div class="val" id="az">0.000</div></div>
  <div class="box"><div class="lbl">|a| (g)</div><div class="val" id="tot">0.000</div></div>
</div>
<div class="two-col">
<div class="collect-box">
  <div class="log-title">THU MẪU TRỰC TIẾP</div>
  <div class="lbl" style="font-size:10px;margin-bottom:4px">NHÃN (LABEL)</div>
  <div class="label-irow">
    <input type="text" id="label-input" value="fall" placeholder="tên nhãn mẫu"
      onkeydown="if(event.key==='Enter')setLabelFromInput()">
    <button onclick="setLabelFromInput()" style="margin-top:0;width:auto;padding:7px 14px;font-size:12px">Lưu</button>
  </div>
  <div style="display:flex;gap:8px;align-items:center">
    <button style="margin-top:0;flex:1;background:#92400e" id="btn-cal" onclick="calibrate()">Calibrate (2s)</button>
    <span class="cal-tag cal-no" id="cal-tag">Uncalibrated</span>
  </div>
  <div class="cal-bar-wrap" id="cal-wrap" style="display:none">
    <div class="cal-bar" id="cal-bar" style="width:0%"></div>
  </div>
  <button id="btn" onclick="trigger()">Bắt đầu thu mẫu</button>
  <div class="auto-row">
    <input type="number" id="auto-count" value="10" min="1" max="500" style="width:75px">
    <button style="margin-top:0;flex:1;background:#166534" id="btn-auto" onclick="autorun()">Auto-run N batch</button>
    <button style="margin-top:0;width:auto;padding:8px 14px;background:#7f1d1d" id="btn-stop" onclick="stoprun()" disabled>Stop</button>
    <span class="auto-progress" id="auto-prog"></span>
  </div>
  <div class="auto-row" style="margin-top:8px">
    <button class="aug-off" id="btn-aug" onclick="toggleAugment()">Scale 3x : OFF</button>
    <button class="jit-off" id="btn-jit" onclick="toggleJitter()">Jitter : OFF</button>
    <span style="font-size:11px;color:#64748b;font-family:'JetBrains Mono',monospace" id="aug-info">
      Scale 3x: gốc x1.0 / nhẹ x0.94 / mạnh x1.06 | Jitter: +/-0.02g, +/-1deg/s
    </span>
    <span style="font-size:11px;font-family:'JetBrains Mono',monospace;color:#a5b4fc" id="aug-prog"></span>
  </div>
</div>
<div class="manual-box" style="margin-top:0">
  <div class="log-title">TĂNG CƯỜNG DỮ LIỆU HÀNG LOẠT (CSV / JSON)</div>
  <div class="manual-file-row">
    <input type="file" id="man-file" accept=".csv,.json,.txt" multiple
      style="color:#94a3b8;font-size:11px;flex:1">
    <input type="text" id="man-label" value="fall" placeholder="label"
      title="Label sẽ gửi lên Edge Impulse" class="manual-file-row">
  </div>
  <div class="manual-hint">Hỗ trợ: CSV (6 cột) • JSON (Edge Impulse format). Chọn nhiều file bằng Ctrl/Shift.</div>
  <div class="aug-cfg-row">
    <div class="cfg-item"><label>Biến thể/file</label>
      <input type="number" id="man-variants" value="5" min="1" max="200"></div>
    <div class="cfg-item"><label>Scale ±%</label>
      <input type="number" id="man-scale-pct" value="8" min="0" max="50"></div>
    <div class="cfg-item"><label>Nhiễu accel (g)</label>
      <input type="number" id="man-jit-acc" value="0.02" min="0" max="1" step="0.005"></div>
    <div class="cfg-item"><label>Nhiễu gyro (°/s)</label>
      <input type="number" id="man-jit-gyro" value="1.0" min="0" max="20" step="0.1"></div>
  </div>
  <div style="display:flex;gap:8px;margin-top:10px">
    <button id="btn-man-up" onclick="uploadManual()"
      style="margin-top:0;flex:1;background:#4c1d95">Bắt đầu Augment &amp; Upload</button>
    <button id="btn-man-stop" onclick="stopManual()"
      style="margin-top:0;width:auto;padding:8px 14px;background:#7f1d1d" disabled>Dừng</button>
  </div>
  <div id="man-prog-wrap" style="display:none;margin-top:8px">
    <div style="display:flex;justify-content:space-between;margin-bottom:4px">
      <span class="man-prog-label" id="man-prog-txt"></span>
      <span style="font-size:11px;color:#a5b4fc;font-family:'JetBrains Mono',monospace;white-space:nowrap"
        id="man-prog-cnt"></span>
    </div>
    <div class="prog-wrap"><div class="prog-bar" id="man-prog-bar"
      style="width:0%;background:#6366f1;transition:width .1s"></div></div>
  </div>
</div>
</div>
<div class="logs-row">
  <div class="log-panel">
    <div class="log-title">Thu thập dữ liệu</div>
    <div class="log log-collect" id="log-collect"></div>
  </div>
  <div class="log-panel">
    <div class="log-title">Serial Monitor</div>
    <div class="log log-serial" id="log-serial"></div>
  </div>
</div>
<script>
const lc=document.getElementById('log-collect');
const ls=document.getElementById('log-serial');
function addCollect(msg){const t=new Date().toTimeString().slice(0,8);lc.textContent+='['+t+'] '+msg+'\n';lc.scrollTop=lc.scrollHeight;}
const STATE_MAP={READY:'READY',CALIBRATING:'CALIBRATING',COLLECTING:'COLLECTING',UPLOADING:'UPLOADING',DONE_OK:'OK',DONE_ERR:'ERROR'};
const STATE_CSS={READY:'state-idle',CALIBRATING:'state-calibrating',COLLECTING:'state-collecting',UPLOADING:'state-uploading',DONE_OK:'state-ok',DONE_ERR:'state-err'};
let prevState='';
async function poll(){
  try{
    const r=await fetch('/status');
    const d=await r.json();
    document.getElementById('ip').textContent='http://'+d.ip;
    document.getElementById('lbl').textContent=d.label;
    const li=document.getElementById('label-input');
    if(li&&document.activeElement!==li)li.value=d.label;
    document.getElementById('ax').textContent=d.aX.toFixed(3);
    document.getElementById('ay').textContent=d.aY.toFixed(3);
    document.getElementById('az').textContent=d.aZ.toFixed(3);
    document.getElementById('tot').textContent=d.totalG.toFixed(3);
    document.getElementById('prog').textContent=d.samples+' / '+d.total;
    document.getElementById('bar').style.width=(d.samples/d.total*100)+'%';
    document.getElementById('last').textContent=d.lastUpload;
    const bs=document.getElementById('box-state');
    bs.className='box '+(STATE_CSS[d.state]||'');
    document.getElementById('st').textContent=STATE_MAP[d.state]||d.state;
    document.getElementById('btn').disabled=(d.state!=='READY');
    document.getElementById('btn-cal').disabled=(d.state!=='READY');
    document.getElementById('btn-auto').disabled=(d.state!=='READY'||d.autoActive);
    document.getElementById('btn-stop').disabled=!d.autoActive;
    const manUp=document.getElementById('btn-man-up');
    if(manUp)manUp.disabled=(d.state!=='READY');
    // Augment Scale toggle button
    const augBtn=document.getElementById('btn-aug');
    augBtn.textContent=d.augment?'Scale 3x : ON':'Scale 3x : OFF';
    augBtn.className=d.augment?'aug-on':'aug-off';
    // Jitter toggle button
    const jitBtn=document.getElementById('btn-jit');
    jitBtn.textContent=d.jitter?'Jitter : ON':'Jitter : OFF';
    jitBtn.className=d.jitter?'jit-on':'jit-off';
    // Augment upload progress badge
    if((d.augment||d.jitter)&&d.state==='UPLOADING'){
      document.getElementById('aug-prog').textContent='['+(d.augIdx+1)+'/'+( d.augment?'3':'1')+']';
    } else {
      document.getElementById('aug-prog').textContent='';
    }
    // Auto-run progress
    if(d.autoActive||d.autoDone>0){
      document.getElementById('auto-prog').textContent=
        'Batch '+(d.autoDone+(d.state==='COLLECTING'||d.state==='UPLOADING'?1:0))+'/'+d.autoTotal;
    } else {
      document.getElementById('auto-prog').textContent='';
    }
    // Cal progress bar
    const isCalib=(d.state==='CALIBRATING');
    document.getElementById('cal-wrap').style.display=isCalib?'block':'none';
    document.getElementById('cal-bar').style.width=(d.calPct||0)+'%';
    // Calibration tag
    const tag=document.getElementById('cal-tag');
    tag.textContent=d.calibrated?'Calibrated':'Uncalibrated';
    tag.className='cal-tag '+(d.calibrated?'cal-ok':'cal-no');
    if(d.state!==prevState){
      addCollect('State → '+d.state);
      if(d.state==='CALIBRATING') addCollect('Calibrating... giữ yên 2 giây');
      if(d.state==='DONE_OK') addCollect('Đã upload: '+d.lastUpload);
      if(d.state==='DONE_ERR') addCollect('Upload thất bại!');
      if(d.state==='COLLECTING') addCollect('Bắt đầu thu '+d.total+' mẫu...');
      if(d.state==='UPLOADING'&&d.augment) addCollect('Upload augment '+d.augIdx+'/3 (scale='+['1.00','0.94','1.06'][d.augIdx]+')');
      prevState=d.state;
    }
    const pct=Math.round(d.samples/d.total*100);
    if(d.state==='COLLECTING'&&d.samples>0&&d.samples%50===0)
      addCollect('> '+d.samples+'/'+d.total+' ('+pct+'%)');
  }catch(e){document.getElementById('st').textContent='Mất kết nối';}
}
async function pollSerial(){
  try{
    const r=await fetch('/log');
    const d=await r.json();
    if(d.lines&&d.lines.length){
      ls.textContent=d.lines.join('\n');
      ls.scrollTop=ls.scrollHeight;
    }
  }catch(e){}
}
async function trigger(){
  document.getElementById('btn').disabled=true;
  addCollect('Gửi lệnh trigger...');
  await fetch('/trigger');
}
async function setLabel(l){
  const r=await fetch('/setlabel?label='+encodeURIComponent(l));
  const t=await r.text();
  const li=document.getElementById('label-input');if(li)li.value=t;
  addCollect('Label -> '+t);
}
async function setLabelFromInput(){
  const l=(document.getElementById('label-input').value||'').trim();
  if(l)await setLabel(l);
}
async function calibrate(){
  addCollect('Calibrating... giu yen tu the dung thang (2s)');
  document.getElementById('btn-cal').disabled=true;
  await fetch('/calibrate');
}
async function autorun(){
  const n=parseInt(document.getElementById('auto-count').value)||1;
  const lbl=(document.getElementById('label-input').value||'?').trim();
  addCollect('Auto-run '+n+' batches (nhan='+lbl+')...');
  document.getElementById('btn-auto').disabled=true;
  await fetch('/autorun?count='+n);
}
async function stoprun(){
  await fetch('/stoprun');
  addCollect('Dừng auto-run.');
}
async function toggleAugment(){
  const r=await fetch('/setaugment');
  const t=await r.text();
  addCollect('Scale 3x -> '+t.toUpperCase()+(t==='on'?' (gốc x1.0 + nhẹ x0.94 + mạnh x1.06)':''));
}
async function toggleJitter(){
  const r=await fetch('/setjitter');
  const t=await r.text();
  addCollect('Jitter -> '+t.toUpperCase()+(t==='on'?' (+/-0.02g / +/-1 deg/s moi mau)':''));
}
let manualRunning=false,manualStop=false;
function parseFileData(text){
  const t=text.trim();
  if(t.startsWith('{')){
    const obj=JSON.parse(t);
    const vals=obj.payload?.values||obj.values;
    if(!vals||!Array.isArray(vals))throw new Error('Không tìm thấy payload.values');
    return vals;
  }
  const rows=[];
  for(const line of t.split('\n')){
    const l=line.trim();
    if(!l||l.startsWith('#'))continue;
    const cols=l.split(',').map(s=>s.trim());
    const nums=cols.map(Number);
    if(!nums.every(n=>!isNaN(n)))continue;
    if(nums.length===6)rows.push(nums);
    else if(nums.length>=7)rows.push(nums.slice(1,7));
  }
  if(!rows.length)throw new Error('Không đọc được dữ liệu CSV');
  return rows;
}
function applyAugmentation(rows,scale,jAcc,jGyro){
  return rows.map(r=>r.map((v,i)=>{
    const n=(Math.random()*2-1)*(i<3?jAcc:jGyro);
    return v*scale+n;
  }));
}
function rowsToCSV(rows){return rows.map(r=>r.map(v=>v.toFixed(4)).join(',')).join('\n');}
async function uploadManual(){
  const fi=document.getElementById('man-file');
  const label=document.getElementById('man-label').value.trim()||'fall';
  const variants=Math.max(1,parseInt(document.getElementById('man-variants').value)||5);
  const scalePct=(parseFloat(document.getElementById('man-scale-pct').value)||8)/100;
  const jAcc=parseFloat(document.getElementById('man-jit-acc').value)||0.02;
  const jGyro=parseFloat(document.getElementById('man-jit-gyro').value)||1.0;
  if(!fi.files.length){addCollect('[LỖI] Chọn file trước!');return;}
  const files=Array.from(fi.files);
  const total=files.length*variants;
  let done=0;
  manualStop=false;manualRunning=true;
  document.getElementById('btn-man-up').disabled=true;
  document.getElementById('btn-man-stop').disabled=false;
  document.getElementById('man-prog-wrap').style.display='block';
  addCollect('Bắt đầu: '+files.length+' file × '+variants+' biến thể = '+total+' mẫu (nhãn='+label+')');
  for(const file of files){
    if(manualStop)break;
    let rows;
    try{
      rows=parseFileData(await file.text());
    }catch(e){addCollect('[LỖI] '+file.name+': '+e.message);continue;}
    for(let v=0;v<variants;v++){
      if(manualStop)break;
      const scale=1+(Math.random()*2-1)*scalePct;
      const csv=rowsToCSV(applyAugmentation(rows,scale,jAcc,jGyro));
      document.getElementById('man-prog-txt').textContent=file.name+' — biến thể '+(v+1)+'/'+variants+' (scale:'+scale.toFixed(3)+')';
      document.getElementById('man-prog-cnt').textContent=(done+1)+'/'+total;
      document.getElementById('man-prog-bar').style.width=((done/total)*100)+'%';
      try{
        let r=await fetch('/upload-manual?label='+encodeURIComponent(label)+'&scale=0&jitter=0',
          {method:'POST',body:csv,headers:{'Content-Type':'text/plain'}});
        if(r.status===400){
          await new Promise(res=>setTimeout(res,2000));
          r=await fetch('/upload-manual?label='+encodeURIComponent(label)+'&scale=0&jitter=0',
            {method:'POST',body:csv,headers:{'Content-Type':'text/plain'}});
        }
        const t=await r.text();
        addCollect('['+(done+1)+'/'+total+'] '+(r.ok?'OK':'FAIL')+' — '+file.name+' scale:'+scale.toFixed(3));
        if(r.ok){
          let waitCount=0;
          while(waitCount<60){
            await new Promise(res=>setTimeout(res,500));
            try{
              const stRes=await fetch('/status');
              const stData=await stRes.json();
              if(stData.state==='READY')break;
            }catch(err){}
            waitCount++;
          }
        }
      }catch(e){addCollect('['+(done+1)+'/'+total+'] LỖI mạng: '+e);}
      done++;
      await new Promise(res=>setTimeout(res,500));
    }
  }
  document.getElementById('man-prog-bar').style.width='100%';
  document.getElementById('man-prog-cnt').textContent=done+'/'+total;
  document.getElementById('man-prog-txt').textContent=manualStop?'Dừng sau '+done+' mẫu.':'Hoàn thành!';
  addCollect(manualStop?'Dừng: '+done+'/'+total+' mẫu.':'Xong! Đã tải '+done+'/'+total+' mẫu lên EI.');
  manualRunning=false;
  document.getElementById('btn-man-up').disabled=false;
  document.getElementById('btn-man-stop').disabled=true;
}
function stopManual(){manualStop=true;addCollect('Dừng sau mẫu hiện tại...');}
setInterval(poll,250);
setInterval(pollSerial,800);
poll();
pollSerial();
</script>
</body></html>
)~~~";
