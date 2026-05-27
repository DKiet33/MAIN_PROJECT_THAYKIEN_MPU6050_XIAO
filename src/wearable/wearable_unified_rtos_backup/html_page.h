#pragma once
#include <pgmspace.h>

// ════════════════════════════════════════════════════════════════
// html_page.h — Web Dashboard Unified (2 tab: Thu mẫu / Suy luận)
// Không dùng emoji. Dark theme. Kế thừa CSS từ wearable_ingestion.
// ════════════════════════════════════════════════════════════════

static const char HTML_PAGE[] PROGMEM = R"~~~(
<!DOCTYPE html><html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wearable Unified</title>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0a0f1a;color:#e2e8f0;font-family:'Inter',sans-serif;padding:14px;font-size:13px}
.header{border-bottom:1px solid #1e293b;padding-bottom:10px;margin-bottom:14px;display:flex;justify-content:space-between;align-items:center}
h1{font-size:14px;color:#38bdf8;font-weight:600;letter-spacing:.04em}
.sub{font-size:11px;color:#64748b;font-family:'JetBrains Mono',monospace}
/* ── Status grid ── */
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:8px;margin-bottom:12px}
.box{background:#111827;padding:10px;border:1px solid #1f2937;border-radius:4px}
.lbl{font-size:10px;color:#6b7280;text-transform:uppercase;letter-spacing:.06em}
.val{font-size:15px;font-weight:600;margin-top:4px;color:#f1f5f9;font-family:'JetBrains Mono',monospace}
/* ── State colours ── */
.state-idle{border-color:#334155;color:#94a3b8}
.state-collecting{background:#0f2a0f;border-color:#22c55e;color:#86efac}
.state-uploading{background:#0f0f2e;border-color:#6366f1;color:#a5b4fc}
.state-ok{background:#064e3b;border-color:#10b981;color:#6ee7b7}
.state-err{background:#7f1d1d;border-color:#ef4444;color:#fca5a5}
.state-calibrating{background:#1c1506;border-color:#f59e0b;color:#fbbf24}
/* ── Progress bar ── */
.prog-wrap{background:#1e293b;height:6px;margin-top:8px;border-radius:3px;overflow:hidden}
.prog-bar{background:#22c55e;height:6px;transition:width .15s;border-radius:3px}
/* ── Buttons ── */
button{display:block;width:100%;padding:11px;margin-top:12px;background:#0284c7;color:#fff;border:none;
  font-family:'Inter',sans-serif;font-size:13px;font-weight:600;cursor:pointer;border-radius:4px;letter-spacing:.04em}
button:hover{background:#0369a1}
button:disabled{background:#1e293b;color:#475569;cursor:not-allowed}
/* ── Tab bar ── */
.tab-bar{display:flex;gap:0;margin-bottom:14px;border-bottom:2px solid #1e293b}
.tab{flex:1;padding:9px 0;background:none;border:none;border-bottom:2px solid transparent;
  margin-bottom:-2px;color:#64748b;font-family:'Inter',sans-serif;font-size:12px;font-weight:600;
  cursor:pointer;letter-spacing:.06em;text-transform:uppercase;border-radius:0;margin-top:0}
.tab:hover{color:#94a3b8;background:none}
.tab:disabled{background:none;color:#64748b;cursor:pointer}
.tab.active{color:#38bdf8;border-bottom-color:#38bdf8}
/* ── Log panels ── */
.logs-row{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:12px}
@media(max-width:600px){.logs-row{grid-template-columns:1fr}}
.log-panel{display:flex;flex-direction:column}
.log-title{font-size:10px;color:#64748b;text-transform:uppercase;letter-spacing:.08em;margin-bottom:5px;font-weight:600}
.log{background:#030712;border:1px solid #1f2937;border-radius:4px;padding:10px;
  height:200px;overflow-y:auto;font-family:'JetBrains Mono',monospace;font-size:11.5px;line-height:1.6;white-space:pre-wrap;word-break:break-all}
.log-collect{color:#86efac}
.log-serial{color:#7dd3fc}
/* ── Calibration ── */
.cal-bar-wrap{background:#1e293b;height:4px;margin-top:6px;border-radius:2px;overflow:hidden}
.cal-bar{background:#f59e0b;height:4px;transition:width .2s;border-radius:2px}
.cal-tag{display:inline-block;font-size:10px;padding:2px 8px;border-radius:10px;font-family:'JetBrains Mono',monospace;margin-left:6px}
.cal-ok{background:#064e3b;color:#6ee7b7;border:1px solid #10b981}
.cal-no{background:#1e293b;color:#64748b;border:1px solid #334155}
/* ── Auto-run row ── */
.auto-row{display:flex;gap:8px;margin-top:12px;align-items:center}
.auto-row input{width:70px;padding:8px;background:#111827;color:#f1f5f9;border:1px solid #334155;
  border-radius:4px;font-family:'JetBrains Mono',monospace;font-size:13px;text-align:center}
.auto-progress{font-size:11px;color:#94a3b8;font-family:'JetBrains Mono',monospace;white-space:nowrap}
/* ── Augmentation buttons ── */
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
/* ── Manual upload box ── */
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
/* ── Two-column layout ── */
.two-col{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:12px;align-items:start}
@media(max-width:800px){.two-col{grid-template-columns:1fr}}
.collect-box{background:#111827;border:1px solid #1f2937;border-radius:6px;padding:12px}
.collect-box .log-title{color:#38bdf8;margin-bottom:10px}
.label-irow{display:flex;gap:8px;align-items:center;margin-bottom:8px}
.label-irow input[type=text]{flex:1;padding:7px 10px;background:#0a0f1a;color:#f1f5f9;
  border:1px solid #334155;border-radius:4px;font-family:'JetBrains Mono',monospace;font-size:13px;font-weight:600;min-width:0}
.label-irow input[type=text]:focus{outline:none;border-color:#38bdf8}
/* ── Inference panel ── */
.infer-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:12px}
@media(max-width:700px){.infer-grid{grid-template-columns:1fr}}
.infer-box{background:#111827;border:1px solid #1f2937;border-radius:6px;padding:14px}
.infer-box .log-title{margin-bottom:10px}
/* Thanh xác suất */
.prob-row{margin-bottom:10px}
.prob-label{display:flex;justify-content:space-between;font-size:11px;font-family:'JetBrains Mono',monospace;margin-bottom:4px}
.prob-bar-wrap{background:#0a0f1a;height:10px;border-radius:5px;overflow:hidden;border:1px solid #1f2937}
.prob-bar{height:10px;border-radius:5px;transition:width .25s;width:0%}
.prob-bar-fall{background:#ef4444}
.prob-bar-idle{background:#64748b}
.prob-bar-walk{background:#22c55e}
/* Nhãn chiếm ưu thế */
.top-label-box{text-align:center;padding:18px 10px;border-radius:6px;border:2px solid #334155;
  background:#0a0f1a;margin-bottom:12px;transition:border-color .3s,background .3s}
.top-label-box .lbl{margin-bottom:6px}
.top-label-text{font-size:32px;font-weight:700;font-family:'JetBrains Mono',monospace;letter-spacing:.04em}
.label-fall{color:#ef4444;border-color:#ef4444;background:#1a0505}
.label-idle{color:#94a3b8;border-color:#334155;background:#0a0f1a}
.label-walk{color:#22c55e;border-color:#22c55e;background:#05130a}
/* Alert log */
.alert-log{background:#030712;border:1px solid #7f1d1d;border-radius:4px;padding:10px;
  height:150px;overflow-y:auto;font-family:'JetBrains Mono',monospace;font-size:11px;
  color:#fca5a5;line-height:1.6;white-space:pre-wrap}
/* Timing strip */
.timing-strip{font-size:11px;color:#64748b;font-family:'JetBrains Mono',monospace;
  margin-top:8px;display:flex;gap:16px}
/* Fall counter */
.fall-counter-row{display:flex;align-items:center;gap:10px;margin-bottom:10px}
.fall-count-num{font-size:28px;font-weight:700;color:#ef4444;font-family:'JetBrains Mono',monospace}
.fall-count-label{font-size:11px;color:#64748b;text-transform:uppercase;letter-spacing:.06em}
</style>
</head>
<body>
<div class="header">
  <h1>WEARABLE UNIFIED</h1>
  <div class="sub" id="ip">--</div>
</div>
<div class="grid">
  <div class="box" id="box-state"><div class="lbl">Trạng thái</div><div class="val" id="st">--</div></div>
  <div class="box"><div class="lbl">Nhãn</div><div class="val" id="lbl">--</div></div>
  <div class="box"><div class="lbl">Tiến độ</div><div class="val" id="prog">0 / 0</div>
    <div class="prog-wrap"><div class="prog-bar" id="bar" style="width:0%"></div></div></div>
  <div class="box"><div class="lbl">Upload gần nhất</div><div class="val" id="last">--</div></div>
</div>
<div class="grid">
  <div class="box"><div class="lbl">aX - Fwd (g)</div><div class="val" id="ax">0.000</div></div>
  <div class="box"><div class="lbl">aY - Left (g)</div><div class="val" id="ay">0.000</div></div>
  <div class="box"><div class="lbl">aZ - Up (g)</div><div class="val" id="az">0.000</div></div>
  <div class="box"><div class="lbl">|a| (g)</div><div class="val" id="tot">0.000</div></div>
</div>
<div class="tab-bar">
  <button class="tab"        id="tab-ingestion" onclick="switchTab('ingestion')">THU MẪU</button>
  <button class="tab active" id="tab-inference" onclick="switchTab('inference')">SUY LUẬN</button>
</div>

<!-- ══════════════════════════════════════════════════════════ -->
<!-- PANEL: THU MAU                                            -->
<!-- ══════════════════════════════════════════════════════════ -->
<div id="panel-ingestion" style="display:none">
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
    <button style="margin-top:0;flex:1;background:#92400e" id="btn-cal" onclick="calibrate()">Hiệu chuẩn (2s)</button>
    <span class="cal-tag cal-no" id="cal-tag">Chưa hiệu chuẩn</span>
  </div>
  <div class="cal-bar-wrap" id="cal-wrap" style="display:none">
    <div class="cal-bar" id="cal-bar" style="width:0%"></div>
  </div>
  <button id="btn" onclick="trigger()">Bắt đầu thu mẫu</button>
  <div class="auto-row">
    <input type="number" id="auto-count" value="10" min="1" max="500" style="width:75px">
    <button style="margin-top:0;flex:1;background:#166534" id="btn-auto" onclick="autorun()">Tự động thu N chuỗi</button>
    <button style="margin-top:0;width:auto;padding:8px 14px;background:#7f1d1d" id="btn-stop" onclick="stoprun()" disabled>Dừng</button>
    <span class="auto-progress" id="auto-prog"></span>
  </div>
  <div class="auto-row" style="margin-top:8px">
    <button class="aug-off" id="btn-aug" onclick="toggleAugment()">Scale 3x : OFF</button>
    <button class="jit-off" id="btn-jit" onclick="toggleJitter()">Jitter : OFF</button>
    <span style="font-size:11px;color:#64748b;font-family:'JetBrains Mono',monospace" id="aug-info">
      Scale 3x: gốc x1.0 / nhẹ x0.94 / mạnh x1.06 | Jitter: ±0.02g, ±1°/s
    </span>
    <span style="font-size:11px;font-family:'JetBrains Mono',monospace;color:#a5b4fc" id="aug-prog"></span>
  </div>
</div>
<div class="manual-box" style="margin-top:0">
  <div class="log-title">TĂNG CƯỜNG DỮ LIỆU HÀNG LOẠT (CSV / JSON)</div>
  <div class="manual-file-row">
    <input type="file" id="man-file" accept=".csv,.json,.txt" multiple
      style="color:#94a3b8;font-size:11px;flex:1">
    <input type="text" id="man-label" value="fall" placeholder="nhãn"
      title="Nhãn sẽ gửi lên Edge Impulse" class="manual-file-row">
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
</div>
<!-- ══════════════════════════════════════════════════════════ -->
<!-- PANEL: SUY LUAN                                           -->
<!-- ══════════════════════════════════════════════════════════ -->
<div id="panel-inference">
<div class="infer-grid">

  <!-- Cot trai: Nhan chiem uu the + Xac suat -->
  <div class="infer-box">
    <div class="log-title">KẾT QUẢ SUY LUẬN</div>
    <div class="top-label-box label-idle" id="top-label-box">
      <div class="lbl">NHÃN HIỆN TẠI</div>
      <div class="top-label-text" id="top-label-text">--</div>
    </div>

    <div class="prob-row">
      <div class="prob-label">
        <span>fall</span><span id="pct-fall">0%</span>
      </div>
      <div class="prob-bar-wrap">
        <div class="prob-bar prob-bar-fall" id="bar-fall" style="width:0%"></div>
      </div>
    </div>
    <div class="prob-row">
      <div class="prob-label">
        <span>idle</span><span id="pct-idle">0%</span>
      </div>
      <div class="prob-bar-wrap">
        <div class="prob-bar prob-bar-idle" id="bar-idle" style="width:0%"></div>
      </div>
    </div>
    <div class="prob-row">
      <div class="prob-label">
        <span>walk</span><span id="pct-walk">0%</span>
      </div>
      <div class="prob-bar-wrap">
        <div class="prob-bar prob-bar-walk" id="bar-walk" style="width:0%"></div>
      </div>
    </div>

    <div class="timing-strip">
      <span>DSP: <span id="t-dsp">--</span>ms</span>
      <span>CLS: <span id="t-cls">--</span>ms</span>
      <span>Total: <span id="t-tot">--</span>ms</span>
    </div>
  </div>

  <!-- Cot phai: Bo dem canh bao + Log canh bao -->
  <div class="infer-box">
    <div class="log-title">CẢNH BÁO TÉ NGÃ</div>
    <div class="fall-counter-row">
      <div class="fall-count-num" id="fall-count">0</div>
      <div>
        <div class="fall-count-label">LẦN PHÁT HIỆN</div>
        <div class="fall-count-label">TRONG PHIÊN NÀY</div>
      </div>
      <button onclick="resetAlerts()"
        style="margin-top:0;width:auto;padding:6px 14px;font-size:11px;background:#1e293b;border:1px solid #334155;margin-left:auto">
        Đặt lại
      </button>
    </div>
    <div class="log-title" style="color:#ef4444;margin-bottom:5px">LOG CẢNH BÁO</div>
    <div class="alert-log" id="alert-log">(Chưa có cảnh báo)</div>
    <div style="margin-top:10px;font-size:10px;color:#64748b;font-family:'JetBrains Mono',monospace">
      Ngưỡng cảnh báo: fall &gt; 85% | Cập nhật mỗi 250ms
    </div>
  </div>

</div>
</div>

<script>
// ── Globals ──────────────────────────────────────────────────
const lc=document.getElementById('log-collect');
const ls=document.getElementById('log-serial');
const alertLog=document.getElementById('alert-log');
let currentTab='inference';
let prevState='';
let alertLogEmpty=true;
let prevFallCount=0;

function ts(){return new Date().toTimeString().slice(0,8);}
function addCollect(msg){lc.textContent+='['+ts()+'] '+msg+'\n';lc.scrollTop=lc.scrollHeight;}
function addAlert(msg){
  if(alertLogEmpty){alertLog.textContent='';alertLogEmpty=false;}
  alertLog.textContent+='['+ts()+'] '+msg+'\n';
  alertLog.scrollTop=alertLog.scrollHeight;
}

// ── Tab switching ─────────────────────────────────────────────
function applyTab(tab){
  // Chi doi giao dien, KHONG gui /setmode len firmware
  currentTab=tab;
  document.getElementById('panel-ingestion').style.display=(tab==='ingestion')?'':'none';
  document.getElementById('panel-inference').style.display=(tab==='inference')?'':'none';
  document.getElementById('tab-ingestion').className='tab'+(tab==='ingestion'?' active':'');
  document.getElementById('tab-inference').className='tab'+(tab==='inference'?' active':'');
}
async function switchTab(tab){
  // Nguoi dung bam nut: doi UI VA bao firmware chuyen mode
  applyTab(tab);
  try{ await fetch('/setmode?mode='+tab); }catch(e){}
}

// ── State maps (Ingestion) ────────────────────────────────────
const STATE_MAP={READY:'Sẵn sàng',CALIBRATING:'Hiệu chuẩn',COLLECTING:'Đang thu',UPLOADING:'Tải lên',DONE_OK:'Thành công',DONE_ERR:'Lỗi'};
const STATE_CSS={READY:'state-idle',CALIBRATING:'state-calibrating',COLLECTING:'state-collecting',UPLOADING:'state-uploading',DONE_OK:'state-ok',DONE_ERR:'state-err'};

// ── Poll ingestion /status ────────────────────────────────────
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
    const augBtn=document.getElementById('btn-aug');
    augBtn.textContent=d.augment?'Scale 3x : ON':'Scale 3x : OFF';
    augBtn.className=d.augment?'aug-on':'aug-off';
    const jitBtn=document.getElementById('btn-jit');
    jitBtn.textContent=d.jitter?'Jitter : ON':'Jitter : OFF';
    jitBtn.className=d.jitter?'jit-on':'jit-off';
    if((d.augment||d.jitter)&&d.state==='UPLOADING'){
      document.getElementById('aug-prog').textContent='['+(d.augIdx+1)+'/'+(d.augment?'3':'1')+']';
    }else{document.getElementById('aug-prog').textContent='';}
    if(d.autoActive||d.autoDone>0){
      document.getElementById('auto-prog').textContent=
        'Batch '+(d.autoDone+(d.state==='COLLECTING'||d.state==='UPLOADING'?1:0))+'/'+d.autoTotal;
    }else{document.getElementById('auto-prog').textContent='';}
    const isCalib=(d.state==='CALIBRATING');
    document.getElementById('cal-wrap').style.display=isCalib?'block':'none';
    document.getElementById('cal-bar').style.width=(d.calPct||0)+'%';
    const tag=document.getElementById('cal-tag');
    tag.textContent=d.calibrated?'Đã hiệu chuẩn':'Chưa hiệu chuẩn';
    tag.className='cal-tag '+(d.calibrated?'cal-ok':'cal-no');
    if(d.state!==prevState){
      addCollect('Trạng thái → '+(STATE_MAP[d.state]||d.state));
      if(d.state==='CALIBRATING')addCollect('Đang hiệu chuẩn... giữ yên 2 giây');
      if(d.state==='DONE_OK')addCollect('Đã tải lên: '+d.lastUpload);
      if(d.state==='DONE_ERR')addCollect('Tải lên thất bại!');
      if(d.state==='COLLECTING')addCollect('Bắt đầu thu '+d.total+' mẫu...');
      prevState=d.state;
    }
    const pct=Math.round(d.samples/d.total*100);
    if(d.state==='COLLECTING'&&d.samples>0&&d.samples%50===0)
      addCollect('> '+d.samples+'/'+d.total+' ('+pct+'%)');
  }catch(e){document.getElementById('st').textContent='Mất kết nối';}
}

// ── Poll inference /infer-status ──────────────────────────────
async function pollInfer(){
  if(currentTab!=='inference')return;
  try{
    const r=await fetch('/infer-status');
    const d=await r.json();
    // Xac suat
    const f=Math.round(d.fall*100),i=Math.round(d.idle*100),w=Math.round(d.walk*100);
    document.getElementById('bar-fall').style.width=f+'%';
    document.getElementById('bar-idle').style.width=i+'%';
    document.getElementById('bar-walk').style.width=w+'%';
    document.getElementById('pct-fall').textContent=f+'%';
    document.getElementById('pct-idle').textContent=i+'%';
    document.getElementById('pct-walk').textContent=w+'%';
    // Top label box
    const box=document.getElementById('top-label-box');
    const txt=document.getElementById('top-label-text');
    txt.textContent=d.label||'--';
    box.className='top-label-box label-'+(d.label||'idle');
    // Timing
    document.getElementById('t-dsp').textContent=d.timing_dsp;
    document.getElementById('t-cls').textContent=d.timing_cls;
    document.getElementById('t-tot').textContent=(d.timing_dsp+d.timing_cls);
    // Fall counter + phat hien canh bao moi
    const newCount=d.fall_count||0;
    document.getElementById('fall-count').textContent=newCount;
    if(newCount>prevFallCount){
      const diff=newCount-prevFallCount;
      for(let k=0;k<diff;k++){
        addAlert('C\u1ea2NH B\u00c1O T\u00c9 NG\u00c3 #'+(prevFallCount+k+1)+' \u2014 fall: '+f+'%');
      }
    }
    prevFallCount=newCount;
    // IP
    document.getElementById('ip').textContent='http://'+d.ip;
  }catch(e){}
}

// ── Serial log ────────────────────────────────────────────────
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

// ── Ingestion actions ─────────────────────────────────────────
async function trigger(){
  document.getElementById('btn').disabled=true;
  addCollect('Gửi lệnh bắt đầu...');
  await fetch('/trigger');
}
async function setLabel(l){
  const r=await fetch('/setlabel?label='+encodeURIComponent(l));
  const t=await r.text();
  const li=document.getElementById('label-input');if(li)li.value=t;
  addCollect('Nhãn → '+t);
}
async function setLabelFromInput(){
  const l=(document.getElementById('label-input').value||'').trim();
  if(l)await setLabel(l);
}
async function calibrate(){
  addCollect('Đang hiệu chuẩn... giữ yên tư thế đứng thẳng (2s)');
  document.getElementById('btn-cal').disabled=true;
  await fetch('/calibrate');
}
async function autorun(){
  const n=parseInt(document.getElementById('auto-count').value)||1;
  const lbl=(document.getElementById('label-input').value||'?').trim();
  addCollect('Tự động thu '+n+' chuỗi (nhãn='+lbl+')...');
  document.getElementById('btn-auto').disabled=true;
  await fetch('/autorun?count='+n);
}
async function stoprun(){ await fetch('/stoprun'); addCollect('Đã dừng tự động thu.'); }
async function toggleAugment(){
  const r=await fetch('/setaugment'); const t=await r.text();
  addCollect('Scale 3x → '+t.toUpperCase()+(t==='on'?' (gốc x1.0 + nhẹ x0.94 + mạnh x1.06)':''));
}
async function toggleJitter(){
  const r=await fetch('/setjitter'); const t=await r.text();
  addCollect('Jitter → '+t.toUpperCase()+(t==='on'?' (±0.02g / ±1 °/s)':''));
}
async function resetAlerts(){
  await fetch('/reset-alerts');
  document.getElementById('fall-count').textContent='0';
  prevFallCount=0;
  alertLog.textContent='(Đã đặt lại)'; alertLogEmpty=false;
}

// ── Manual upload (batch augmentation) ───────────────────────
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
  if(!fi.files.length){addCollect('[LỖI] Vui lòng chọn file trước!');return;}
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
    try{rows=parseFileData(await file.text());}
    catch(e){addCollect('[LỖI] '+file.name+': '+e.message);continue;}
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
        addCollect('['+(done+1)+'/'+total+'] '+(r.ok?'OK':'FAIL')+' — '+file.name+' scale:'+scale.toFixed(3));
        if(r.ok){
          let wc=0;
          while(wc<60){
            await new Promise(res=>setTimeout(res,500));
            try{const st=await(await fetch('/status')).json();if(st.state==='READY')break;}catch(err){}
            wc++;
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

// ── Start polling ─────────────────────────────────────────────
// Luon reset ve mode Suy luan moi khi tai/reload trang
switchTab('inference');
setInterval(poll,250);
setInterval(pollInfer,250);
setInterval(pollSerial,800);
poll(); pollInfer(); pollSerial();
</script>
</body></html>
)~~~";