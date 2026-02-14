#pragma once

const char WEB_UI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Output Controller</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: #1a1a2e; color: #eee; padding: 16px;
  }
  h1 { text-align: center; margin-bottom: 16px; color: #e94560; }
  .grid {
    display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
    gap: 12px; max-width: 1200px; margin: 0 auto;
  }
  .output-card {
    background: #16213e; border-radius: 8px; padding: 12px;
    border: 1px solid #0f3460;
  }
  .output-card .header {
    display: flex; justify-content: space-between; align-items: center;
    margin-bottom: 8px;
  }
  .output-card .name { font-weight: 600; font-size: 14px; }
  .output-card .angle {
    font-size: 20px; font-weight: 700; color: #e94560;
    min-width: 42px; text-align: right;
  }
  .output-card input[type=range] {
    width: 100%; height: 6px; -webkit-appearance: none; appearance: none;
    background: #0f3460; border-radius: 3px; outline: none;
  }
  .output-card input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none; width: 20px; height: 20px;
    background: #e94560; border-radius: 50%; cursor: pointer;
  }
  .output-card select {
    background: #0f3460; color: #eee; border: 1px solid #1a3a5c;
    border-radius: 4px; padding: 2px 4px; font-size: 11px; cursor: pointer;
    margin-right: 8px;
  }
  .controls {
    max-width: 1200px; margin: 20px auto 12px;
    display: flex; flex-wrap: wrap; gap: 8px; justify-content: center;
  }
  .controls button {
    padding: 10px 20px; border: none; border-radius: 6px;
    font-size: 14px; font-weight: 600; cursor: pointer;
    transition: opacity 0.2s;
  }
  .controls button:hover { opacity: 0.85; }
  .btn-rec { background: #e94560; color: #fff; }
  .btn-stop { background: #666; color: #fff; }
  .btn-play { background: #0f3460; color: #fff; }
  .btn-save { background: #533483; color: #fff; }
  .seq-section {
    max-width: 1200px; margin: 16px auto;
    background: #16213e; border-radius: 8px; padding: 12px;
    border: 1px solid #0f3460;
  }
  .seq-section h3 { margin-bottom: 8px; }
  .seq-list { list-style: none; }
  .seq-list li {
    display: flex; justify-content: space-between; align-items: center;
    padding: 6px 0; border-bottom: 1px solid #0f3460;
  }
  .seq-list li button {
    padding: 4px 12px; border: none; border-radius: 4px;
    cursor: pointer; font-size: 12px; margin-left: 6px;
  }
  .seq-play { background: #0f3460; color: #fff; }
  .seq-del { background: #e94560; color: #fff; }
  #status {
    text-align: center; margin: 8px auto; font-size: 13px; color: #aaa;
  }
  .btn-env {
    padding: 2px 8px; border: 1px solid #e94560; border-radius: 4px;
    background: transparent; color: #e94560; cursor: pointer;
    font-size: 11px; font-weight: 600; margin-right: 8px;
    transition: background 0.2s;
  }
  .btn-env:hover, .btn-env.active { background: #e94560; color: #fff; }
  .envelope-panel {
    display: none; margin-top: 10px; border-top: 1px solid #0f3460;
    padding-top: 10px;
  }
  .envelope-panel.show { display: block; }
  .env-canvas {
    width: 100%; height: 150px; background: #0d1b36; border-radius: 4px;
    cursor: crosshair; touch-action: none;
  }
  .env-controls {
    display: flex; align-items: center; gap: 8px; margin-top: 8px;
    flex-wrap: wrap; font-size: 12px;
  }
  .env-controls label { display: flex; align-items: center; gap: 4px; }
  .env-controls input[type=range] {
    width: 80px; height: 4px; -webkit-appearance: none; appearance: none;
    background: #0f3460; border-radius: 2px;
  }
  .env-controls input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none; width: 14px; height: 14px;
    background: #e94560; border-radius: 50%; cursor: pointer;
  }
  .env-controls input[type=checkbox] { accent-color: #e94560; }
  .env-controls button {
    padding: 4px 12px; border: none; border-radius: 4px;
    font-size: 12px; font-weight: 600; cursor: pointer;
  }
  .btn-env-play { background: #0f3460; color: #fff; }
  .btn-env-stop { background: #e94560; color: #fff; }
  .output-card .input-row {
    display: flex; align-items: center; gap: 6px; margin-bottom: 6px; font-size: 11px;
  }
  .output-card .input-row label { color: #888; }
  .output-card .input-row select {
    background: #0f3460; color: #eee; border: 1px solid #1a3a5c;
    border-radius: 4px; padding: 2px 4px; font-size: 11px; cursor: pointer;
  }
  .output-card.ext-input input[type=range] { opacity: 0.4; pointer-events: none; }
  .ota-section {
    max-width: 1200px; margin: 16px auto;
    background: #16213e; border-radius: 8px; padding: 12px;
    border: 1px solid #0f3460;
  }
  .ota-section h3 { margin-bottom: 8px; }
  .ota-form { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
  .ota-form input[type=file] { color: #aaa; font-size: 13px; }
  .btn-ota {
    padding: 6px 16px; border: none; border-radius: 4px;
    background: #533483; color: #fff; font-size: 13px;
    font-weight: 600; cursor: pointer;
  }
  .btn-ota:disabled { opacity: 0.5; cursor: not-allowed; }
  #otaProgress {
    width: 100%; height: 6px; background: #0f3460; border-radius: 3px;
    margin-top: 8px; display: none;
  }
  #otaProgress .bar {
    height: 100%; background: #e94560; border-radius: 3px;
    transition: width 0.2s;
  }
</style>
</head>
<body>
<h1>Output Controller</h1>
<div id="status">Connecting...</div>
<div class="controls">
  <button class="btn-rec" onclick="seqRecord()">Record</button>
  <button class="btn-stop" onclick="seqStop()">Stop</button>
  <button class="btn-play" onclick="seqPlayLast()">Play Last</button>
  <button class="btn-save" onclick="seqSave()">Save</button>
</div>
<div class="grid" id="outputs"></div>
<div class="seq-section">
  <h3>Saved Sequences</h3>
  <ul class="seq-list" id="seqList"><li>Loading...</li></ul>
</div>
<div class="ota-section">
  <h3>Firmware Update</h3>
  <div class="ota-form">
    <input type="file" id="otaFile" accept=".bin">
    <button class="btn-ota" id="otaBtn" onclick="otaUpload()">Upload</button>
    <span id="otaStatus" style="font-size:13px;color:#aaa;"></span>
  </div>
  <div id="otaProgress"><div class="bar" id="otaBar" style="width:0%"></div></div>
</div>

<script>
const NUM = 16;
let angles = new Array(NUM).fill(90);
let types = new Array(NUM).fill('servo');
let inputs = new Array(NUM).fill('manual');
let envelopes = [];
for (let i = 0; i < NUM; i++) {
  envelopes.push({
    points: [{t: 0, a: 90}, {t: 2.0, a: 90}],
    duration: 2.0, loop: false, playing: false,
    timer: null, startTime: 0, expanded: false,
    dragging: -1, canvasReady: false,
    rafId: null, sendTimer: null
  });
}

function formatValue(ch, val) {
  if (types[ch] === 'motor' || types[ch] === 'lego' || types[ch] === 'pwm' || types[ch] === 'motor1k') {
    return Math.round(val * 100 / 180) + '%';
  }
  return val + '\u00B0';
}

function init() {
  const grid = document.getElementById('outputs');
  for (let i = 0; i < NUM; i++) {
    const card = document.createElement('div');
    card.className = 'output-card';
    card.innerHTML = `
      <div class="header">
        <span class="name" id="name${i}">Output ${i}</span>
        <span style="display:flex;align-items:center;">
          <select id="type${i}" onchange="onTypeChange(${i}, this.value)">
            <option value="servo">Servo</option>
            <option value="motor">Motor</option>
            <option value="lego">Lego</option>
            <option value="pwm">PWM</option>
            <option value="motor1k">Motor 1kHz</option>
          </select>
          <button class="btn-env" id="envBtn${i}" onclick="toggleEnvelope(${i})">Env</button>
          <span class="angle" id="val${i}">90</span>
        </span>
      </div>
      <div class="input-row">
        <label>Input:</label>
        <select id="in${i}" onchange="onInputChange(${i}, this.value)">
          <option value="manual">Manual</option>
          <option value="envelope">Envelope</option>
          <option value="pot">Pot</option>
          <option value="ps4_lx">PS4 L-Stick X</option>
          <option value="ps4_ly">PS4 L-Stick Y</option>
          <option value="ps4_rx">PS4 R-Stick X</option>
          <option value="ps4_ry">PS4 R-Stick Y</option>
          <option value="ps4_l2">PS4 L2</option>
          <option value="ps4_r2">PS4 R2</option>
          <option value="ps4_cross">Cross</option>
          <option value="ps4_circle">Circle</option>
          <option value="ps4_square">Square</option>
          <option value="ps4_triangle">Triangle</option>
          <option value="ps4_l1">PS4 L1</option>
          <option value="ps4_r1">PS4 R1</option>
        </select>
      </div>
      <input type="range" min="0" max="180" value="90" id="sl${i}"
        oninput="onSlider(${i}, this.value)">
      <div class="envelope-panel" id="envPanel${i}">
        <canvas class="env-canvas" id="envCanvas${i}"></canvas>
        <div class="env-controls">
          <button class="btn-env-play" id="envPlayBtn${i}" onclick="envTogglePlay(${i})">Play</button>
          <label>Dur: <input type="range" min="0.5" max="30" step="0.5" value="2"
            oninput="envSetDuration(${i}, this.value)"> <span id="envDur${i}">2.0s</span></label>
          <label><input type="checkbox" onchange="envSetLoop(${i}, this.checked)"> Loop</label>
        </div>
      </div>
    `;
    grid.appendChild(card);
  }
  fetchState();
  loadSequences();
}

function fetchState() {
  fetch('/api/outputs').then(r => r.json()).then(data => {
    if (Array.isArray(data)) {
      data.forEach((s, i) => {
        if (i < NUM) {
          angles[i] = s.angle;
          document.getElementById('sl' + i).value = s.angle;
          if (s.type) {
            types[i] = s.type;
            document.getElementById('type' + i).value = s.type;
          }
          document.getElementById('val' + i).textContent = formatValue(i, s.angle);
          if (s.name) document.getElementById('name' + i).textContent = s.name;
          if (s.input) {
            inputs[i] = s.input;
            document.getElementById('in' + i).value = s.input;
            updateInputStyle(i);
          }
        }
      });
    }
    document.getElementById('status').textContent = 'Connected';
  }).catch(() => {
    document.getElementById('status').textContent = 'Connection failed - retrying...';
    setTimeout(fetchState, 3000);
  });
}

function onTypeChange(ch, type) {
  types[ch] = type;
  var defVal = (type === 'servo') ? 90 : 0;
  angles[ch] = defVal;
  document.getElementById('sl' + ch).value = defVal;
  document.getElementById('val' + ch).textContent = formatValue(ch, defVal);
  fetch('/api/output/type', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({channel: ch, type: type})
  }).then(() => {
    fetch('/api/output', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({channel: ch, angle: defVal})
    }).catch(() => {});
  }).catch(() => {});
}

function onInputChange(ch, src) {
  inputs[ch] = src;
  updateInputStyle(ch);
  fetch('/api/output/input', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({channel: ch, input: src})
  }).catch(() => {});
}

function updateInputStyle(ch) {
  var card = document.getElementById('sl' + ch).closest('.output-card');
  if (inputs[ch] !== 'manual') {
    card.classList.add('ext-input');
  } else {
    card.classList.remove('ext-input');
  }
}

let sendTimer = null;
let pendingChannel = -1;
let pendingAngle = 0;

function onSlider(ch, val) {
  val = parseInt(val);
  angles[ch] = val;
  document.getElementById('val' + ch).textContent = formatValue(ch, val);

  pendingChannel = ch;
  pendingAngle = val;
  if (!sendTimer) {
    sendTimer = setTimeout(() => {
      fetch('/api/output', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({channel: pendingChannel, angle: pendingAngle})
      }).catch(() => {});
      sendTimer = null;
    }, 50);
  }
}

function seqRecord() {
  fetch('/api/sequence/record', {method:'POST'}).then(() => {
    document.getElementById('status').textContent = 'Recording...';
  });
}
function seqStop() {
  fetch('/api/sequence/stop', {method:'POST'}).then(() => {
    document.getElementById('status').textContent = 'Stopped';
  });
}
function seqPlayLast() {
  fetch('/api/sequence/play', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({name:'_last'})
  }).then(() => {
    document.getElementById('status').textContent = 'Playing...';
  });
}
function seqSave() {
  const name = prompt('Sequence name:');
  if (!name) return;
  fetch('/api/sequence/save', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({name: name})
  }).then(() => {
    document.getElementById('status').textContent = 'Saved: ' + name;
    loadSequences();
  });
}
function seqPlay(name) {
  fetch('/api/sequence/play', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({name: name})
  }).then(() => {
    document.getElementById('status').textContent = 'Playing: ' + name;
  });
}
function seqDelete(name) {
  if (!confirm('Delete "' + name + '"?')) return;
  fetch('/api/sequence/delete', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({name: name})
  }).then(() => loadSequences());
}
function loadSequences() {
  fetch('/api/sequences').then(r => r.json()).then(list => {
    const ul = document.getElementById('seqList');
    if (!list.length) { ul.innerHTML = '<li>No saved sequences</li>'; return; }
    ul.innerHTML = list.map(s =>
      `<li><span>${s}</span><span>
        <button class="seq-play" onclick="seqPlay('${s}')">Play</button>
        <button class="seq-del" onclick="seqDelete('${s}')">Del</button>
      </span></li>`
    ).join('');
  }).catch(() => {});
}

// --- Envelope Curve Editor ---
function toggleEnvelope(ch) {
  var env = envelopes[ch];
  env.expanded = !env.expanded;
  var panel = document.getElementById('envPanel' + ch);
  var btn = document.getElementById('envBtn' + ch);
  if (env.expanded) {
    panel.classList.add('show');
    btn.classList.add('active');
    if (!env.canvasReady) { initEnvCanvas(ch); env.canvasReady = true; }
    env.points = [{t: 0, a: angles[ch]}, {t: env.duration, a: angles[ch]}];
    drawEnvelope(ch);
  } else {
    panel.classList.remove('show');
    btn.classList.remove('active');
    if (env.playing) envStop(ch);
  }
}

function initEnvCanvas(ch) {
  var canvas = document.getElementById('envCanvas' + ch);
  canvas.addEventListener('mousedown', function(e) { envMouseDown(ch, e); });
  canvas.addEventListener('mousemove', function(e) { envMouseMove(ch, e); });
  canvas.addEventListener('mouseup', function(e) { envMouseUp(ch); });
  canvas.addEventListener('mouseleave', function(e) { envMouseUp(ch); });
  canvas.addEventListener('dblclick', function(e) { envDblClick(ch, e); });
  canvas.addEventListener('touchstart', function(e) { e.preventDefault(); envMouseDown(ch, e.touches[0]); }, {passive: false});
  canvas.addEventListener('touchmove', function(e) { e.preventDefault(); envMouseMove(ch, e.touches[0]); }, {passive: false});
  canvas.addEventListener('touchend', function(e) { envMouseUp(ch); });
}

var ENV_PAD = 30, ENV_PADR = 10, ENV_PADT = 10, ENV_PADB = 15;

function envCanvasCoords(ch, clientX, clientY) {
  var canvas = document.getElementById('envCanvas' + ch);
  var rect = canvas.getBoundingClientRect();
  var env = envelopes[ch];
  var x = clientX - rect.left;
  var y = clientY - rect.top;
  var cw = canvas.width - ENV_PAD - ENV_PADR;
  var ch2 = canvas.height - ENV_PADT - ENV_PADB;
  var t = ((x - ENV_PAD) / cw) * env.duration;
  var a = (1 - (y - ENV_PADT) / ch2) * 180;
  return {
    t: Math.max(0, Math.min(env.duration, t)),
    a: Math.max(0, Math.min(180, Math.round(a))),
    px: x, py: y
  };
}

function envPointToCanvas(ch, t, a) {
  var canvas = document.getElementById('envCanvas' + ch);
  var env = envelopes[ch];
  var cw = canvas.width - ENV_PAD - ENV_PADR;
  var ch2 = canvas.height - ENV_PADT - ENV_PADB;
  return {
    x: ENV_PAD + (t / env.duration) * cw,
    y: ENV_PADT + (1 - a / 180) * ch2
  };
}

function envFindPoint(ch, px, py) {
  var env = envelopes[ch];
  for (var i = 0; i < env.points.length; i++) {
    var p = envPointToCanvas(ch, env.points[i].t, env.points[i].a);
    var dx = px - p.x, dy = py - p.y;
    if (dx * dx + dy * dy < 144) return i;
  }
  return -1;
}

function envMouseDown(ch, e) {
  var env = envelopes[ch];
  var c = envCanvasCoords(ch, e.clientX, e.clientY);
  var idx = envFindPoint(ch, c.px, c.py);
  if (idx >= 0) {
    env.dragging = idx;
  } else {
    var np = {t: c.t, a: c.a};
    env.points.push(np);
    env.points.sort(function(a, b) { return a.t - b.t; });
    env.dragging = env.points.indexOf(np);
    drawEnvelope(ch);
  }
}

function envMouseMove(ch, e) {
  var env = envelopes[ch];
  if (env.dragging < 0) return;
  var pt = env.points[env.dragging];
  var c = envCanvasCoords(ch, e.clientX, e.clientY);
  pt.t = c.t;
  pt.a = c.a;
  env.points.sort(function(a, b) { return a.t - b.t; });
  env.dragging = env.points.indexOf(pt);
  drawEnvelope(ch);
}

function envMouseUp(ch) {
  envelopes[ch].dragging = -1;
}

function envDblClick(ch, e) {
  var env = envelopes[ch];
  if (env.points.length <= 2) return;
  var c = envCanvasCoords(ch, e.clientX, e.clientY);
  var idx = envFindPoint(ch, c.px, c.py);
  if (idx >= 0) {
    env.points.splice(idx, 1);
    drawEnvelope(ch);
  }
}

function drawEnvelope(ch) {
  var canvas = document.getElementById('envCanvas' + ch);
  var w = canvas.clientWidth;
  if (w < 1) return;
  if (canvas.width !== w) canvas.width = w;
  canvas.height = 150;
  var ctx = canvas.getContext('2d');
  var env = envelopes[ch];
  var W = canvas.width, H = canvas.height;
  var cw = W - ENV_PAD - ENV_PADR;
  var ch2 = H - ENV_PADT - ENV_PADB;

  ctx.clearRect(0, 0, W, H);

  // Y grid (0, 45, 90, 135, 180)
  ctx.strokeStyle = '#1a3a5c';
  ctx.lineWidth = 0.5;
  ctx.font = '9px sans-serif';
  ctx.fillStyle = '#556';
  for (var a = 0; a <= 180; a += 45) {
    var y = ENV_PADT + (1 - a / 180) * ch2;
    ctx.beginPath(); ctx.moveTo(ENV_PAD, y); ctx.lineTo(W - ENV_PADR, y); ctx.stroke();
    ctx.fillText(a + '\u00B0', 2, y + 3);
  }

  // X grid
  var xStep;
  if (env.duration <= 3) xStep = 0.5;
  else if (env.duration <= 10) xStep = 1;
  else if (env.duration <= 20) xStep = 2;
  else xStep = 5;
  var xSteps = Math.round(env.duration / xStep);
  for (var si = 0; si <= xSteps; si++) {
    var gt = si * xStep;
    var x = ENV_PAD + (gt / env.duration) * cw;
    ctx.beginPath(); ctx.moveTo(x, ENV_PADT); ctx.lineTo(x, H - ENV_PADB); ctx.stroke();
    ctx.fillText(gt.toFixed(1) + 's', x - 10, H - 2);
  }

  // Curve line
  if (env.points.length > 1) {
    ctx.strokeStyle = '#e94560';
    ctx.lineWidth = 2;
    ctx.beginPath();
    for (var i = 0; i < env.points.length; i++) {
      var p = envPointToCanvas(ch, env.points[i].t, env.points[i].a);
      if (i === 0) ctx.moveTo(p.x, p.y);
      else ctx.lineTo(p.x, p.y);
    }
    ctx.stroke();
  }

  // Loop wrap-around line
  if (env.loop && env.points.length > 1) {
    var last = env.points[env.points.length - 1];
    var first = env.points[0];
    var pLast = envPointToCanvas(ch, last.t, last.a);
    var pWrap = envPointToCanvas(ch, env.duration, first.a);
    ctx.strokeStyle = '#e9456080';
    ctx.lineWidth = 1.5;
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    ctx.moveTo(pLast.x, pLast.y);
    ctx.lineTo(pWrap.x, pWrap.y);
    ctx.stroke();
    ctx.setLineDash([]);
  }

  // Control points
  for (var i = 0; i < env.points.length; i++) {
    var p = envPointToCanvas(ch, env.points[i].t, env.points[i].a);
    ctx.beginPath();
    ctx.arc(p.x, p.y, 6, 0, Math.PI * 2);
    ctx.fillStyle = (i === env.dragging) ? '#fff' : '#e94560';
    ctx.fill();
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 1.5;
    ctx.stroke();
  }

  // Playback cursor
  if (env.playing) {
    var elapsed = (performance.now() - env.startTime) / 1000;
    var ct = env.loop ? (elapsed % env.duration) : Math.min(elapsed, env.duration);
    var cx = ENV_PAD + (ct / env.duration) * cw;
    ctx.strokeStyle = '#4fc3f7';
    ctx.lineWidth = 2;
    ctx.beginPath(); ctx.moveTo(cx, ENV_PADT); ctx.lineTo(cx, H - ENV_PADB); ctx.stroke();
  }
}

function envSetDuration(ch, val) {
  val = parseFloat(val);
  var env = envelopes[ch];
  var oldDur = env.duration;
  if (oldDur > 0) {
    var scale = val / oldDur;
    env.points.forEach(function(p) { p.t = Math.round(p.t * scale * 1000) / 1000; });
  }
  env.duration = val;
  document.getElementById('envDur' + ch).textContent = val.toFixed(1) + 's';
  drawEnvelope(ch);
}

function envSetLoop(ch, val) {
  envelopes[ch].loop = val;
}

function envInterpolate(env, t) {
  var pts = env.points;
  if (pts.length === 0) return 90;
  if (pts.length === 1) return pts[0].a;
  if (env.loop) {
    pts = pts.slice();
    var last = pts[pts.length - 1];
    if (last.t < env.duration - 0.001) {
      pts.push({t: env.duration, a: pts[0].a});
    }
  }
  if (t <= pts[0].t) return pts[0].a;
  if (t >= pts[pts.length - 1].t) return pts[pts.length - 1].a;
  for (var i = 0; i < pts.length - 1; i++) {
    if (t >= pts[i].t && t <= pts[i + 1].t) {
      var span = pts[i + 1].t - pts[i].t;
      var frac = span > 0 ? (t - pts[i].t) / span : 0;
      return Math.round(pts[i].a + frac * (pts[i + 1].a - pts[i].a));
    }
  }
  return pts[pts.length - 1].a;
}

function envTogglePlay(ch) {
  if (envelopes[ch].playing) envStop(ch);
  else envPlay(ch);
}

function envPlay(ch) {
  var env = envelopes[ch];
  if (env.points.length < 2) return;
  env.playing = true;
  env.startTime = performance.now();
  var btn = document.getElementById('envPlayBtn' + ch);
  btn.textContent = 'Stop';
  btn.className = 'btn-env-stop';

  function frame() {
    if (!env.playing) return;
    var elapsed = (performance.now() - env.startTime) / 1000;
    var t = env.loop ? (elapsed % env.duration) : elapsed;
    if (!env.loop && t > env.duration) { envStop(ch); return; }
    var angle = envInterpolate(env, t);
    document.getElementById('sl' + ch).value = angle;
    document.getElementById('val' + ch).textContent = formatValue(ch, angle);
    angles[ch] = angle;
    drawEnvelope(ch);
    env.rafId = requestAnimationFrame(frame);
  }

  function sendLoop() {
    if (!env.playing) return;
    var elapsed = (performance.now() - env.startTime) / 1000;
    var t = env.loop ? (elapsed % env.duration) : Math.min(elapsed, env.duration);
    var angle = envInterpolate(env, t);
    fetch('/api/output', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({channel: ch, angle: angle})
    }).catch(function() {}).finally(function() {
      if (env.playing) env.sendTimer = setTimeout(sendLoop, 50);
    });
  }

  env.rafId = requestAnimationFrame(frame);
  sendLoop();
}

function envStop(ch) {
  var env = envelopes[ch];
  env.playing = false;
  if (env.rafId) { cancelAnimationFrame(env.rafId); env.rafId = null; }
  if (env.sendTimer) { clearTimeout(env.sendTimer); env.sendTimer = null; }
  if (env.timer) { clearInterval(env.timer); env.timer = null; }
  var btn = document.getElementById('envPlayBtn' + ch);
  btn.textContent = 'Play';
  btn.className = 'btn-env-play';
  drawEnvelope(ch);
}

function otaUpload() {
  var file = document.getElementById('otaFile').files[0];
  if (!file) { document.getElementById('otaStatus').textContent = 'Select a .bin file'; return; }
  var btn = document.getElementById('otaBtn');
  var status = document.getElementById('otaStatus');
  var prog = document.getElementById('otaProgress');
  var bar = document.getElementById('otaBar');
  btn.disabled = true;
  prog.style.display = 'block';
  bar.style.width = '0%';
  status.textContent = 'Uploading...';

  var form = new FormData();
  form.append('firmware', file);
  var xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/ota');
  xhr.upload.onprogress = function(e) {
    if (e.lengthComputable) {
      var pct = Math.round(e.loaded * 100 / e.total);
      bar.style.width = pct + '%';
      status.textContent = 'Uploading... ' + pct + '%';
    }
  };
  xhr.onload = function() {
    if (xhr.status === 200) {
      bar.style.width = '100%';
      status.textContent = 'Done! Rebooting...';
      setTimeout(function() { location.reload(); }, 5000);
    } else {
      status.textContent = 'Failed: ' + xhr.responseText;
      btn.disabled = false;
    }
  };
  xhr.onerror = function() {
    status.textContent = 'Upload error';
    btn.disabled = false;
  };
  xhr.send(form);
}

init();
</script>
</body>
</html>
)rawliteral";
