#pragma once

const char WEB_UI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Output Controller</title>
<style>
  :root { --accent: #e94560; }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: #1a1a2e; color: #eee; padding: 16px;
  }
  h1 { text-align: center; margin-bottom: 16px; color: var(--accent); }
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
    font-size: 20px; font-weight: 700; color: var(--accent);
    min-width: 42px; text-align: right;
  }
  .output-card input[type=range] {
    width: 100%; height: 6px; -webkit-appearance: none; appearance: none;
    background: #0f3460; border-radius: 3px; outline: none;
  }
  .output-card input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none; width: 20px; height: 20px;
    background: var(--accent); border-radius: 50%; cursor: pointer;
  }
  .output-card select {
    background: #0f3460; color: #eee; border: 1px solid #1a3a5c;
    border-radius: 4px; padding: 2px 4px; font-size: 11px; cursor: pointer;
    margin-right: 8px;
  }
  .btn-rec { background: var(--accent); color: #fff; }
  .btn-save { background: #533483; color: #fff; }
  .section {
    max-width: 1200px; margin: 16px auto;
    background: #16213e; border-radius: 8px; padding: 12px;
    border: 1px solid #0f3460;
  }
  .section h3 { margin-bottom: 8px; }
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
  .seq-del { background: var(--accent); color: #fff; }
  #status {
    text-align: center; margin: 8px auto; font-size: 13px; color: #aaa;
  }
  .input-panel { display: none; margin-top: 8px; }
  .input-panel.show { display: block; }
  .envelope-panel {
    border-top: 1px solid #0f3460; padding-top: 10px;
  }
  .seq-panel {
    border-top: 1px solid #0f3460; padding-top: 10px;
  }
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
    background: var(--accent); border-radius: 50%; cursor: pointer;
  }
  .env-controls input[type=checkbox] { accent-color: var(--accent); }
  .env-controls button {
    padding: 4px 12px; border: none; border-radius: 4px;
    font-size: 12px; font-weight: 600; cursor: pointer;
  }
  .env-controls select {
    background: #0f3460; color: #eee; border: 1px solid #1a3a5c;
    border-radius: 4px; padding: 2px 4px; font-size: 11px; cursor: pointer;
  }
  .btn-env-play { background: #0f3460; color: #fff; }
  .btn-env-stop { background: var(--accent); color: #fff; }
  .output-card .input-row {
    display: flex; align-items: center; gap: 6px; margin-bottom: 6px; font-size: 11px;
  }
  .output-card .input-row label { color: #888; }
  .output-card .input-row select {
    background: #0f3460; color: #eee; border: 1px solid #1a3a5c;
    border-radius: 4px; padding: 2px 4px; font-size: 11px; cursor: pointer;
  }
  .pot-panel select {
    background: #0f3460; color: #eee; border: 1px solid #1a3a5c;
    border-radius: 4px; padding: 4px 8px; font-size: 12px; cursor: pointer;
  }
  .pot-panel label { font-size: 12px; color: #aaa; }
  .ext-slider input[type=range] { opacity: 0.4; pointer-events: none; }
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
    height: 100%; background: var(--accent); border-radius: 3px;
    transition: width 0.2s;
  }
</style>
</head>
<body>
<h1>Output Controller</h1>
<div id="status">Connecting...</div>
<div class="grid" id="outputs"></div>
<div class="section">
  <h3>Presets</h3>
  <div style="display:flex;gap:8px;margin-bottom:8px;">
    <button class="btn-save" onclick="presetSave()">Save Preset</button>
  </div>
  <ul class="seq-list" id="presetList"><li>Loading...</li></ul>
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
// Random accent color on each load
var accentHue = Math.floor(Math.random() * 360);
var accentColor = 'hsl(' + accentHue + ',75%,55%)';
document.documentElement.style.setProperty('--accent', accentColor);

const NUM = 16;
const POT_PINS = [34, 35, 32, 33];
let angles = new Array(NUM).fill(90);
let types = new Array(NUM).fill('servo');
let inputs = new Array(NUM).fill('manual');
let pots = new Array(NUM).fill(0);
let envelopes = [];
for (let i = 0; i < NUM; i++) {
  envelopes.push({
    points: [{t: 0, a: 90}, {t: 2.0, a: 90}],
    duration: 2.0, loop: false, playing: false,
    timer: null, startTime: 0,
    dragging: -1, canvasReady: false,
    rafId: null, sendTimer: null
  });
}
let sequences = [];
for (let i = 0; i < NUM; i++) {
  sequences.push({
    points: [], duration: 0, loop: false,
    playing: false, recording: false,
    timeScale: 1.0, startTime: 0,
    rafId: null, sendTimer: null, recTimer: null,
    pollTimer: null, recInput: 'manual'
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
    card.id = 'card' + i;
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
          <span class="angle" id="val${i}">90</span>
        </span>
      </div>
      <div class="input-row">
        <label>Input:</label>
        <select id="in${i}" onchange="onInputChange(${i}, this.value)">
          <option value="manual">Manual</option>
          <option value="envelope">Envelope</option>
          <option value="sequence">Sequence</option>
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
      <div class="input-panel show" id="panelManual${i}">
        <input type="range" min="0" max="180" value="90" id="sl${i}"
          oninput="onSlider(${i}, this.value)">
      </div>
      <div class="input-panel pot-panel" id="panelPot${i}">
        <label>Pot: <select id="pot${i}" onchange="onPotChange(${i}, this.value)">
          ${POT_PINS.map((p, pi) => '<option value="'+pi+'">Pot '+pi+' (GPIO'+p+')</option>').join('')}
        </select></label>
      </div>
      <div class="input-panel envelope-panel" id="panelEnv${i}">
        <canvas class="env-canvas" id="envCanvas${i}"></canvas>
        <div class="env-controls">
          <button class="btn-env-play" id="envPlayBtn${i}" onclick="envTogglePlay(${i})">Play</button>
          <label>Dur: <input type="range" min="0.5" max="30" step="0.5" value="2"
            oninput="envSetDuration(${i}, this.value)"> <span id="envDur${i}">2.0s</span></label>
          <label><input type="checkbox" onchange="envSetLoop(${i}, this.checked)"> Loop</label>
        </div>
        <div class="env-controls" style="margin-top:4px;">
          <button class="btn-save" onclick="envSaveCh(${i})" style="font-size:11px;padding:3px 8px;">Save</button>
          <select id="envSel${i}"><option value="">-- saved --</option></select>
          <button class="seq-play" onclick="envLoadCh(${i})" style="font-size:11px;padding:3px 8px;">Load</button>
          <button class="seq-del" onclick="envDelCh(${i})" style="font-size:11px;padding:3px 8px;">Del</button>
        </div>
      </div>
      <div class="input-panel ext-slider" id="panelExt${i}">
        <input type="range" min="0" max="180" value="90" id="extSl${i}" disabled>
      </div>
      <div class="input-panel seq-panel" id="panelSeq${i}">
        <input type="range" min="0" max="180" value="90" id="seqSl${i}"
          oninput="onSlider(${i}, this.value)">
        <canvas class="env-canvas" id="seqCanvas${i}" style="margin-top:6px;cursor:default;"></canvas>
        <div class="env-controls">
          <button class="btn-rec" id="seqRecBtn${i}" onclick="seqToggleRec(${i})">Rec</button>
          <button class="btn-env-play" id="seqPlayBtn${i}" onclick="seqTogglePlay(${i})">Play</button>
          <label><input type="checkbox" id="seqLoop${i}" onchange="seqSetLoop(${i}, this.checked)"> Loop</label>
          <label>Speed: <input type="range" min="25" max="400" step="25" value="100"
            id="seqSpeed${i}" oninput="seqSetSpeed(${i}, this.value)"> <span id="seqSpeedVal${i}">1.0x</span></label>
        </div>
        <div class="env-controls" style="margin-top:4px;">
          <label>Rec from: <select id="seqIn${i}">
            <option value="manual">Slider</option>
            <option value="pot_0">Pot 0</option>
            <option value="pot_1">Pot 1</option>
            <option value="pot_2">Pot 2</option>
            <option value="pot_3">Pot 3</option>
            <option value="ps4_lx">PS4 LX</option>
            <option value="ps4_ly">PS4 LY</option>
            <option value="ps4_rx">PS4 RX</option>
            <option value="ps4_ry">PS4 RY</option>
            <option value="ps4_l2">PS4 L2</option>
            <option value="ps4_r2">PS4 R2</option>
            <option value="ps4_cross">Cross</option>
            <option value="ps4_circle">Circle</option>
            <option value="ps4_square">Square</option>
            <option value="ps4_triangle">Tri</option>
            <option value="ps4_l1">PS4 L1</option>
            <option value="ps4_r1">PS4 R1</option>
          </select></label>
        </div>
        <div class="env-controls" style="margin-top:4px;">
          <button class="btn-save" onclick="seqSaveCh(${i})" style="font-size:11px;padding:3px 8px;">Save</button>
          <select id="seqSel${i}"><option value="">-- saved --</option></select>
          <button class="seq-play" onclick="seqLoadCh(${i})" style="font-size:11px;padding:3px 8px;">Load</button>
          <button class="seq-del" onclick="seqDelCh(${i})" style="font-size:11px;padding:3px 8px;">Del</button>
        </div>
      </div>
    `;
    grid.appendChild(card);
  }
  fetchState();
  loadPresets();
  loadSeqListAll();
  loadEnvListAll();
}

function updateCardUI(ch) {
  var src = inputs[ch];
  document.getElementById('panelManual' + ch).classList.remove('show');
  document.getElementById('panelPot' + ch).classList.remove('show');
  document.getElementById('panelEnv' + ch).classList.remove('show');
  document.getElementById('panelExt' + ch).classList.remove('show');
  document.getElementById('panelSeq' + ch).classList.remove('show');

  if (src === 'manual') {
    document.getElementById('panelManual' + ch).classList.add('show');
  } else if (src === 'pot') {
    document.getElementById('panelPot' + ch).classList.add('show');
    document.getElementById('pot' + ch).value = pots[ch];
  } else if (src === 'envelope') {
    document.getElementById('panelEnv' + ch).classList.add('show');
    var env = envelopes[ch];
    if (!env.canvasReady) { initEnvCanvas(ch); env.canvasReady = true; }
    env.points = [{t: 0, a: angles[ch]}, {t: env.duration, a: angles[ch]}];
    drawEnvelope(ch);
  } else if (src === 'sequence') {
    document.getElementById('panelSeq' + ch).classList.add('show');
    document.getElementById('seqSl' + ch).value = angles[ch];
    drawSequence(ch);
  } else {
    // PS4 or other external input
    document.getElementById('panelExt' + ch).classList.add('show');
    document.getElementById('extSl' + ch).value = angles[ch];
  }
}

function fetchState() {
  fetch('/api/outputs').then(r => r.json()).then(data => {
    if (Array.isArray(data)) {
      data.forEach((s, i) => {
        if (i < NUM) {
          angles[i] = s.angle;
          document.getElementById('sl' + i).value = s.angle;
          document.getElementById('extSl' + i).value = s.angle;
          document.getElementById('seqSl' + i).value = s.angle;
          if (s.type) {
            types[i] = s.type;
            document.getElementById('type' + i).value = s.type;
          }
          document.getElementById('val' + i).textContent = formatValue(i, s.angle);
          if (s.name) document.getElementById('name' + i).textContent = s.name;
          if (s.input) {
            inputs[i] = s.input;
            document.getElementById('in' + i).value = s.input;
          }
          if (s.pot !== undefined) pots[i] = s.pot;
          updateCardUI(i);
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
  updateCardUI(ch);
  fetch('/api/input', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({channel: ch, input: src})
  }).catch(() => {});
}

function onPotChange(ch, potIdx) {
  pots[ch] = parseInt(potIdx);
  fetch('/api/pot-assign', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({channel: ch, pot: pots[ch]})
  }).catch(() => {});
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

// --- Per-Channel Sequence Recording & Playback ---

function seqToggleRec(ch) {
  if (sequences[ch].recording) seqStopRec(ch);
  else seqStartRec(ch);
}

function seqStartRec(ch) {
  var seq = sequences[ch];
  if (seq.playing) seqStopPlay(ch);
  seq.points = [];
  seq.duration = 0;
  seq.recording = true;
  seq.startTime = performance.now();
  seq.recInput = document.getElementById('seqIn' + ch).value;
  var btn = document.getElementById('seqRecBtn' + ch);
  btn.textContent = 'Stop';
  btn.className = 'btn-env-stop';
  // If recording from external input, set ESP input and start polling
  if (seq.recInput !== 'manual') {
    document.getElementById('seqSl' + ch).disabled = true;
    if (seq.recInput.startsWith('pot_')) {
      var potIdx = parseInt(seq.recInput.charAt(4));
      fetch('/api/input', {method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({channel: ch, input: 'pot'})}).catch(function(){});
      fetch('/api/pot-assign', {method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({channel: ch, pot: potIdx})}).catch(function(){});
    } else {
      fetch('/api/input', {method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({channel: ch, input: seq.recInput})}).catch(function(){});
    }
    seqPollLoop(ch);
  }
  // Capture value every 50ms
  seq.recTimer = setInterval(function() {
    var elapsed = (performance.now() - seq.startTime) / 1000;
    seq.points.push({t: elapsed, a: angles[ch]});
    seq.duration = elapsed;
    document.getElementById('seqSl' + ch).value = angles[ch];
    document.getElementById('val' + ch).textContent = formatValue(ch, angles[ch]);
    drawSequence(ch);
  }, 50);
}

function seqStopRec(ch) {
  var seq = sequences[ch];
  if (!seq.recording) return;
  seq.recording = false;
  if (seq.recTimer) { clearInterval(seq.recTimer); seq.recTimer = null; }
  if (seq.pollTimer) { clearTimeout(seq.pollTimer); seq.pollTimer = null; }
  // Capture final point
  var elapsed = (performance.now() - seq.startTime) / 1000;
  seq.points.push({t: elapsed, a: angles[ch]});
  seq.duration = elapsed;
  // If was recording from external input, switch back to sequence
  if (seq.recInput !== 'manual') {
    document.getElementById('seqSl' + ch).disabled = false;
    fetch('/api/input', {method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({channel: ch, input: 'sequence'})}).catch(function(){});
  }
  var btn = document.getElementById('seqRecBtn' + ch);
  btn.textContent = 'Rec';
  btn.className = 'btn-rec';
  drawSequence(ch);
}

function seqPollLoop(ch) {
  if (!sequences[ch].recording) return;
  fetch('/api/outputs').then(function(r) { return r.json(); }).then(function(data) {
    if (Array.isArray(data) && data[ch]) {
      angles[ch] = data[ch].angle;
    }
    if (sequences[ch].recording) sequences[ch].pollTimer = setTimeout(function() { seqPollLoop(ch); }, 40);
  }).catch(function() {
    if (sequences[ch].recording) sequences[ch].pollTimer = setTimeout(function() { seqPollLoop(ch); }, 100);
  });
}

function seqTogglePlay(ch) {
  if (sequences[ch].playing) seqStopPlay(ch);
  else seqStartPlay(ch);
}

function seqStartPlay(ch) {
  var seq = sequences[ch];
  if (seq.recording) seqStopRec(ch);
  if (seq.points.length < 2) return;
  seq.playing = true;
  seq.startTime = performance.now();
  var btn = document.getElementById('seqPlayBtn' + ch);
  btn.textContent = 'Stop';
  btn.className = 'btn-env-stop';

  // Build points with timeScale applied to time axis
  var scaledDur = seq.duration / seq.timeScale;
  var pts = seq.points.map(function(p) { return {t: p.t / seq.timeScale, a: p.a}; });

  // Send all points to ESP for smooth ~1kHz interpolation
  fetch('/api/curve-play', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({channel: ch, duration: scaledDur, loop: seq.loop, points: pts})
  }).catch(function() {});

  // RAF loop only for visual cursor animation (no motor control)
  function frame() {
    if (!seq.playing) return;
    var elapsed = (performance.now() - seq.startTime) / 1000 * seq.timeScale;
    var t = seq.loop ? (elapsed % seq.duration) : elapsed;
    if (!seq.loop && t > seq.duration) { seqStopPlay(ch); return; }
    var angle = seqInterpolate(seq, t);
    document.getElementById('seqSl' + ch).value = angle;
    document.getElementById('val' + ch).textContent = formatValue(ch, angle);
    angles[ch] = angle;
    drawSequence(ch);
    seq.rafId = requestAnimationFrame(frame);
  }
  seq.rafId = requestAnimationFrame(frame);
}

function seqStopPlay(ch) {
  var seq = sequences[ch];
  seq.playing = false;
  if (seq.rafId) { cancelAnimationFrame(seq.rafId); seq.rafId = null; }
  if (seq.sendTimer) { clearTimeout(seq.sendTimer); seq.sendTimer = null; }
  // Stop ESP-side curve playback
  fetch('/api/curve-stop', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({channel: ch})
  }).catch(function() {});
  var btn = document.getElementById('seqPlayBtn' + ch);
  btn.textContent = 'Play';
  btn.className = 'btn-env-play';
  drawSequence(ch);
}

function seqInterpolate(seq, t) {
  var pts = seq.points;
  if (pts.length === 0) return 90;
  if (pts.length === 1) return pts[0].a;
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

function seqSetLoop(ch, val) { sequences[ch].loop = val; }

function seqSetSpeed(ch, val) {
  var s = parseInt(val) / 100;
  sequences[ch].timeScale = s;
  document.getElementById('seqSpeedVal' + ch).textContent = (s % 1 === 0 ? s.toFixed(1) : String(s)) + 'x';
}

function seqSaveCh(ch) {
  var seq = sequences[ch];
  if (seq.points.length < 2) { alert('No sequence recorded'); return; }
  var name = prompt('Sequence name:');
  if (!name) return;
  fetch('/api/seq-save', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({ channel: ch, name: name, points: seq.points })
  }).then(function() {
    document.getElementById('status').textContent = 'Seq saved: ' + name;
    loadSeqListAll();
  });
}

function seqLoadCh(ch) {
  var sel = document.getElementById('seqSel' + ch);
  var name = sel.value;
  if (!name) return;
  fetch('/api/seq-load', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({name: name})
  }).then(function(r) { return r.json(); }).then(function(data) {
    var seq = sequences[ch];
    if (seq.playing) seqStopPlay(ch);
    seq.points = data.pts.map(function(p) { return {t: p[0], a: p[1]}; });
    seq.duration = seq.points.length > 0 ? seq.points[seq.points.length - 1].t : 0;
    drawSequence(ch);
    document.getElementById('status').textContent = 'Seq loaded: ' + name;
  });
}

function seqDelCh(ch) {
  var sel = document.getElementById('seqSel' + ch);
  var name = sel.value;
  if (!name) return;
  if (!confirm('Delete "' + name + '"?')) return;
  fetch('/api/seq-del', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({name: name})
  }).then(function() { loadSeqListAll(); });
}

function loadSeqListAll() {
  fetch('/api/seq-list').then(function(r) { return r.json(); }).then(function(list) {
    for (var ch = 0; ch < NUM; ch++) {
      var sel = document.getElementById('seqSel' + ch);
      if (!sel) continue;
      var cur = sel.value;
      sel.innerHTML = '<option value="">-- saved --</option>';
      for (var j = 0; j < list.length; j++) {
        var opt = document.createElement('option');
        opt.value = list[j]; opt.textContent = list[j];
        sel.appendChild(opt);
      }
      if (cur) sel.value = cur;
    }
  }).catch(function() {});
}

function drawSequence(ch) {
  var canvas = document.getElementById('seqCanvas' + ch);
  var w = canvas.clientWidth;
  if (w < 1) return;
  if (canvas.width !== w) canvas.width = w;
  canvas.height = 150;
  var ctx = canvas.getContext('2d');
  var seq = sequences[ch];
  var W = canvas.width, H = canvas.height;
  var PAD_L = 30, PAD_R = 10, PAD_T = 10, PAD_B = 15;
  var cw = W - PAD_L - PAD_R;
  var ch2 = H - PAD_T - PAD_B;
  var dur = seq.duration > 0 ? seq.duration : 1;

  ctx.clearRect(0, 0, W, H);

  // Y grid (0, 45, 90, 135, 180)
  ctx.strokeStyle = '#1a3a5c';
  ctx.lineWidth = 0.5;
  ctx.font = '9px sans-serif';
  ctx.fillStyle = '#556';
  for (var a = 0; a <= 180; a += 45) {
    var y = PAD_T + (1 - a / 180) * ch2;
    ctx.beginPath(); ctx.moveTo(PAD_L, y); ctx.lineTo(W - PAD_R, y); ctx.stroke();
    ctx.fillText(a + '\u00B0', 2, y + 3);
  }

  // X grid
  var xStep;
  if (dur <= 3) xStep = 0.5;
  else if (dur <= 10) xStep = 1;
  else if (dur <= 20) xStep = 2;
  else xStep = 5;
  var xSteps = Math.round(dur / xStep);
  for (var si = 0; si <= xSteps; si++) {
    var gt = si * xStep;
    var x = PAD_L + (gt / dur) * cw;
    ctx.beginPath(); ctx.moveTo(x, PAD_T); ctx.lineTo(x, H - PAD_B); ctx.stroke();
    ctx.fillText(gt.toFixed(1) + 's', x - 10, H - 2);
  }

  // Curve line
  if (seq.points.length > 1) {
    ctx.strokeStyle = accentColor;
    ctx.lineWidth = 2;
    ctx.beginPath();
    for (var i = 0; i < seq.points.length; i++) {
      var px = PAD_L + (seq.points[i].t / dur) * cw;
      var py = PAD_T + (1 - seq.points[i].a / 180) * ch2;
      if (i === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.stroke();
  }

  // Empty state text
  if (seq.points.length === 0 && !seq.recording) {
    ctx.fillStyle = '#445';
    ctx.font = '13px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText('No sequence - press Rec or Load', W / 2, H / 2);
    ctx.textAlign = 'start';
  }

  // Playback cursor
  if (seq.playing) {
    var elapsed = (performance.now() - seq.startTime) / 1000 * seq.timeScale;
    var ct = seq.loop ? (elapsed % dur) : Math.min(elapsed, dur);
    var cx = PAD_L + (ct / dur) * cw;
    ctx.strokeStyle = '#4fc3f7';
    ctx.lineWidth = 2;
    ctx.beginPath(); ctx.moveTo(cx, PAD_T); ctx.lineTo(cx, H - PAD_B); ctx.stroke();
  }

  // Recording indicator
  if (seq.recording) {
    ctx.fillStyle = accentColor;
    ctx.font = 'bold 11px sans-serif';
    ctx.fillText('REC ' + seq.duration.toFixed(1) + 's', PAD_L + 4, PAD_T + 14);
  }
}

// --- Presets ---
function presetSave() {
  const name = prompt('Preset name:');
  if (!name) return;
  fetch('/api/preset-save', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({name: name})
  }).then(() => {
    document.getElementById('status').textContent = 'Preset saved: ' + name;
    loadPresets();
  });
}
function presetLoad(name) {
  fetch('/api/preset-load', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({name: name})
  }).then(() => {
    document.getElementById('status').textContent = 'Preset loaded: ' + name;
    fetchState();
  });
}
function presetDelete(name) {
  if (!confirm('Delete preset "' + name + '"?')) return;
  fetch('/api/preset-del', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({name: name})
  }).then(() => loadPresets());
}
function loadPresets() {
  fetch('/api/presets').then(r => r.json()).then(list => {
    const ul = document.getElementById('presetList');
    if (!list.length) { ul.innerHTML = '<li>No saved presets</li>'; return; }
    ul.innerHTML = list.map(s =>
      `<li><span>${s}</span><span>
        <button class="seq-play" onclick="presetLoad('${s}')">Load</button>
        <button class="seq-del" onclick="presetDelete('${s}')">Del</button>
      </span></li>`
    ).join('');
  }).catch(() => {});
}

// --- Envelope Curve Editor ---
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
    ctx.strokeStyle = accentColor;
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
    ctx.strokeStyle = 'hsla(' + accentHue + ',75%,55%,0.5)';
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
    ctx.fillStyle = (i === env.dragging) ? '#fff' : accentColor;
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

function envSaveCh(ch) {
  var env = envelopes[ch];
  if (env.points.length < 2) { alert('No envelope to save'); return; }
  var name = prompt('Envelope name:');
  if (!name) return;
  fetch('/api/env-save', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({name: name, duration: env.duration, loop: env.loop, points: env.points})
  }).then(function() {
    document.getElementById('status').textContent = 'Envelope saved: ' + name;
    loadEnvListAll();
  });
}

function envLoadCh(ch) {
  var sel = document.getElementById('envSel' + ch);
  var name = sel.value;
  if (!name) return;
  fetch('/api/env-load', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({name: name})
  }).then(function(r) { return r.json(); }).then(function(data) {
    var env = envelopes[ch];
    if (env.playing) envStop(ch);
    env.points = data.pts.map(function(p) { return {t: p[0], a: p[1]}; });
    env.duration = data.dur || (env.points.length > 0 ? env.points[env.points.length - 1].t : 2);
    env.loop = data.loop || false;
    document.getElementById('envDur' + ch).textContent = env.duration.toFixed(1) + 's';
    drawEnvelope(ch);
    document.getElementById('status').textContent = 'Envelope loaded: ' + name;
  });
}

function envDelCh(ch) {
  var sel = document.getElementById('envSel' + ch);
  var name = sel.value;
  if (!name) return;
  if (!confirm('Delete "' + name + '"?')) return;
  fetch('/api/env-del', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({name: name})
  }).then(function() { loadEnvListAll(); });
}

function loadEnvListAll() {
  fetch('/api/env-list').then(function(r) { return r.json(); }).then(function(list) {
    for (var ch = 0; ch < NUM; ch++) {
      var sel = document.getElementById('envSel' + ch);
      if (!sel) continue;
      var cur = sel.value;
      sel.innerHTML = '<option value="">-- saved --</option>';
      for (var j = 0; j < list.length; j++) {
        var opt = document.createElement('option');
        opt.value = list[j]; opt.textContent = list[j];
        sel.appendChild(opt);
      }
      if (cur) sel.value = cur;
    }
  }).catch(function() {});
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

  // Build points for ESP-side curve playback
  var pts = env.points.map(function(p) { return {t: p.t, a: p.a}; });
  // For loop mode, add wrap-around point back to first value
  if (env.loop && pts.length > 0) {
    var last = pts[pts.length - 1];
    if (last.t < env.duration - 0.001) {
      pts.push({t: env.duration, a: pts[0].a});
    }
  }
  // Send all points to ESP for smooth ~1kHz interpolation
  fetch('/api/curve-play', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({channel: ch, duration: env.duration, loop: env.loop, points: pts})
  }).catch(function() {});

  // RAF loop only for visual cursor animation (no motor control)
  function frame() {
    if (!env.playing) return;
    var elapsed = (performance.now() - env.startTime) / 1000;
    var t = env.loop ? (elapsed % env.duration) : elapsed;
    if (!env.loop && t > env.duration) { envStop(ch); return; }
    var angle = envInterpolate(env, t);
    document.getElementById('val' + ch).textContent = formatValue(ch, angle);
    angles[ch] = angle;
    drawEnvelope(ch);
    env.rafId = requestAnimationFrame(frame);
  }
  env.rafId = requestAnimationFrame(frame);
}

function envStop(ch) {
  var env = envelopes[ch];
  env.playing = false;
  if (env.rafId) { cancelAnimationFrame(env.rafId); env.rafId = null; }
  if (env.sendTimer) { clearTimeout(env.sendTimer); env.sendTimer = null; }
  if (env.timer) { clearInterval(env.timer); env.timer = null; }
  // Stop ESP-side curve playback
  fetch('/api/curve-stop', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({channel: ch})
  }).catch(function() {});
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
