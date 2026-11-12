# -*- coding: utf-8 -*-
"""
OBJ 모델을 품은 단독 실행 3D 뷰어(HTML)를 만든다. 외부 라이브러리, 서버 불필요.
모델을 하나씩 골라 자유 시점으로 본다.

실행:  python Tools/make_viewer.py
출력:  Assets/Models/viewer.html   (더블클릭하면 브라우저에서 열림)

모델을 다시 생성했으면 이 스크립트도 다시 실행해야 뷰어에 반영된다.
"""
import base64
import json
import os

# (파일 이름, 뷰어에 표시할 이름)
MODEL_LIST = [
    ('stadium', '경기장'),
    ('hole', '수비 구멍'),
    ('ball', '공'),
    ('bat', '배트'),
    ('player_red', '선수 (빨강)'),
    ('player_blue', '선수 (파랑)'),
]

HTML = r"""<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>토이 베이스볼 모델 뷰어</title>
<style>
  html, body { margin: 0; height: 100%; overflow: hidden; background: #dcefe6;
               font: 14px/1.4 "Malgun Gothic", "Segoe UI", sans-serif; color: #17303a; }
  canvas { display: block; width: 100%; height: 100%; cursor: grab; touch-action: none; }
  canvas:active { cursor: grabbing; }
  #bar { position: fixed; left: 12px; top: 12px; display: flex; flex-direction: column; gap: 6px;
         max-width: calc(100% - 24px); }
  .row { display: flex; flex-wrap: wrap; gap: 6px; align-items: center; }
  button, label.chk { border: 1px solid #9fb8b0; background: #ffffffd9; border-radius: 6px;
         padding: 6px 10px; font: inherit; color: inherit; cursor: pointer; }
  button:hover { background: #fff; }
  button.on { background: #17303a; border-color: #17303a; color: #fff; }
  label.chk input { vertical-align: -2px; margin: 0 4px 0 0; }
  #info { position: fixed; left: 12px; bottom: 10px; right: 12px; width: fit-content;
          background: #ffffffc0; border-radius: 6px; padding: 5px 9px; font-size: 12px; }
</style>
</head>
<body>
<canvas id="c"></canvas>
<div id="bar">
  <div class="row" id="models"></div>
  <div class="row">
    <button data-view="home">기본</button>
    <button data-view="top">위</button>
    <button data-view="front">앞</button>
    <button data-view="side">옆</button>
    <button data-view="back">뒤</button>
    <label class="chk"><input type="checkbox" id="axes" checked>원점 축</label>
    <label class="chk"><input type="checkbox" id="cull" checked>뒷면 제거</label>
    <label class="chk"><input type="checkbox" id="wire">와이어프레임</label>
    <label class="chk"><input type="checkbox" id="tex" checked>텍스처</label>
  </div>
</div>
<div id="info"></div>
<script>
const MODELS = __MODELS__;

function parse(objText, mtlText) {
  const mats = {}; let cur = null;
  for (const line of mtlText.split('\n')) {
    const s = line.trim().split(/\s+/);
    if (s[0] === 'newmtl') { cur = mats[s[1]] = { kd: [.8, .8, .8], ke: [0, 0, 0] }; }
    else if (s[0] === 'Kd') cur.kd = s.slice(1, 4).map(Number);
    else if (s[0] === 'Ke') cur.ke = s.slice(1, 4).map(Number);
  }
  const V = [], VN = [], VT = [], pos = [], nor = [], col = [], emi = [], bary = [], uv = [];
  const lo = [1e9, 1e9, 1e9], hi = [-1e9, -1e9, -1e9];
  let m = { kd: [.8, .8, .8], ke: [0, 0, 0] };
  for (const line of objText.split('\n')) {
    const s = line.trim().split(/\s+/);
    if (s[0] === 'v') {
      const p = [+s[1], +s[2], +s[3]]; V.push(p);
      for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], p[k]); hi[k] = Math.max(hi[k], p[k]); }
    }
    else if (s[0] === 'vn') VN.push([+s[1], +s[2], +s[3]]);
    else if (s[0] === 'vt') VT.push([+s[1], +s[2]]);
    else if (s[0] === 'usemtl') m = mats[s[1]] || m;
    else if (s[0] === 'f') {
      const idx = s.slice(1).map(t => parseInt(t.split('/')[0]) - 1);
      const nix = s.slice(1).map(t => parseInt(t.split('/')[2]) - 1);
      const tix = s.slice(1).map(t => parseInt(t.split('/')[1]) - 1);
      for (let i = 1; i < idx.length - 1; i++) {
        const a = V[idx[0]], b = V[idx[i]], c = V[idx[i + 1]];
        const ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
        const vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
        let nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const l = Math.hypot(nx, ny, nz) || 1; nx /= l; ny /= l; nz /= l;
        [a, b, c].forEach((p, k) => {
          const vn = VN[nix[[0, i, i + 1][k]]];
          const vt = VT[tix[[0, i, i + 1][k]]] || [0, 0];
          uv.push(vt[0], vt[1]);
          pos.push(p[0], p[1], p[2]);
          if (vn) nor.push(vn[0], vn[1], vn[2]); else nor.push(nx, ny, nz);
          col.push(m.kd[0], m.kd[1], m.kd[2]); emi.push(m.ke[0], m.ke[1], m.ke[2]);
          bary.push(k === 0 ? 1 : 0, k === 1 ? 1 : 0, k === 2 ? 1 : 0);
        });
      }
    }
  }
  return { pos, nor, col, emi, bary, uv, count: pos.length / 3, lo, hi };
}

const canvas = document.getElementById('c');
const gl = canvas.getContext('webgl', { antialias: true });
gl.getExtension('OES_standard_derivatives');

const VS = `
attribute vec3 aPos, aNor, aCol, aEmi, aBary;
attribute vec2 aUV;
uniform mat4 uVP;
varying vec3 vNor, vCol, vEmi, vBary;
varying vec2 vUV;
void main() {
  vUV = aUV; vNor = aNor; vCol = aCol; vEmi = aEmi; vBary = aBary;
  gl_Position = uVP * vec4(aPos, 1.0);
}`;
const FS = `
#extension GL_OES_standard_derivatives : enable
precision mediump float;
varying vec3 vNor, vCol, vEmi, vBary;
varying vec2 vUV;
uniform float uWire, uFlat, uUseTex;
uniform sampler2D uTex;
void main() {
  vec3 c = vCol;
  vec3 k = vCol;
  if (uUseTex > 0.5) k *= texture2D(uTex, vUV).rgb;
  if (uFlat < 0.5) {
    vec3 n = normalize(vNor);
    if (!gl_FrontFacing) n = -n;
    float d = max(dot(n, normalize(vec3(0.35, 0.85, 0.40))), 0.0);
    c = min(k * (0.42 + 0.58 * d) + vEmi * 0.5, 1.0);
    if (uWire > 0.5) {
      vec3 w = fwidth(vBary);
      vec3 e = smoothstep(vec3(0.0), w * 1.2, vBary);
      c = mix(vec3(0.05, 0.12, 0.15), c, min(min(e.x, e.y), e.z));
    }
  }
  gl_FragColor = vec4(c, 1.0);
}`;
function shader(type, src) {
  const s = gl.createShader(type); gl.shaderSource(s, src); gl.compileShader(s);
  if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s));
  return s;
}
const prog = gl.createProgram();
gl.attachShader(prog, shader(gl.VERTEX_SHADER, VS));
gl.attachShader(prog, shader(gl.FRAGMENT_SHADER, FS));
gl.linkProgram(prog); gl.useProgram(prog);
const uVP = gl.getUniformLocation(prog, 'uVP');
const uWire = gl.getUniformLocation(prog, 'uWire');
const uFlat = gl.getUniformLocation(prog, 'uFlat');
const uUseTex = gl.getUniformLocation(prog, 'uUseTex');

const KEYS = ['pos', 'nor', 'col', 'emi', 'bary', 'uv'];
const ATTR = { pos: 'aPos', nor: 'aNor', col: 'aCol', emi: 'aEmi', bary: 'aBary', uv: 'aUV' };
function upload(mesh) {
  const bufs = {};
  for (const k of KEYS) {
    bufs[k] = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, bufs[k]);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(mesh[k]), gl.STATIC_DRAW);
  }
  return bufs;
}
function draw(bufs, count, mode) {
  for (const k of KEYS) {
    const loc = gl.getAttribLocation(prog, ATTR[k]);
    gl.bindBuffer(gl.ARRAY_BUFFER, bufs[k]);
    gl.enableVertexAttribArray(loc);
    gl.vertexAttribPointer(loc, k === 'uv' ? 2 : 3, gl.FLOAT, false, 0, 0);
  }
  gl.drawArrays(mode, 0, count);
}

// ---- 모델 준비: 각각 따로 올리고, 크기에 맞춘 시점과 원점 축을 만든다 ----
const ITEMS = MODELS.list.map(([name, label]) => {
  const mesh = parse(MODELS[name + '_obj'], MODELS[name + '_mtl']);
  const size = mesh.hi.map((v, k) => v - mesh.lo[k]);
  const center = mesh.hi.map((v, k) => (v + mesh.lo[k]) / 2);
  const R = Math.hypot(...size) / 2;
  const L = R * 0.7;                                   // 축 길이
  const ax = { pos: [], nor: [], col: [], emi: [], bary: [], uv: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] };
  [[1, 0, 0, [0.85, 0.2, 0.2]], [0, 1, 0, [0.2, 0.65, 0.25]], [0, 0, 1, [0.2, 0.35, 0.9]]].forEach(([x, y, z, c]) => {
    ax.pos.push(0, 0, 0, x * L, y * L, z * L);
    for (let k = 0; k < 2; k++) { ax.nor.push(0, 1, 0); ax.col.push(...c); ax.emi.push(0, 0, 0); ax.bary.push(1, 1, 1); }
  });
  // 텍스처 (map_Kd 가 있는 모델만). OBJ 규칙대로 v = 0 이 이미지 아래쪽
  let tex = null;
  if (MODELS[name + '_tex']) {
    tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array([255, 255, 255, 255]));
    const im = new Image();
    im.onload = () => {
      gl.bindTexture(gl.TEXTURE_2D, tex);
      gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, im);
      gl.generateMipmap(gl.TEXTURE_2D);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
    };
    im.src = MODELS[name + '_tex'];
  }
  return { name, label, bufs: upload(mesh), count: mesh.count, size, center, R, axes: upload(ax), tex };
});

let item = null, cam = null;
function view(kind) {
  const it = item, d = it.R * 2.9;
  const v = {
    home:  { yaw: 30,  pitch: 24 },
    top:   { yaw: 0,   pitch: 89 },
    front: { yaw: 0,   pitch: 6 },
    side:  { yaw: 90,  pitch: 6 },
    back:  { yaw: 180, pitch: 14 },
  }[kind];
  // 경기장의 기본 시점은 게임의 고정 사선 시점
  if (kind === 'home' && it.name === 'stadium') return { t: [0, 0, -14], yaw: 0, pitch: 39.5, dist: 51.9 };
  return { t: it.center.slice(), yaw: v.yaw, pitch: v.pitch, dist: d };
}
function select(i) {
  item = ITEMS[i];
  cam = view('home');
  document.querySelectorAll('#models button').forEach((b, k) => b.classList.toggle('on', k === i));
}
ITEMS.forEach((it, i) => {
  const b = document.createElement('button');
  b.textContent = it.label;
  b.addEventListener('click', () => select(i));
  document.getElementById('models').appendChild(b);
});
select(0);

// ---- 카메라 (타깃 중심 궤도) ----
function eye() {
  const y = cam.yaw * Math.PI / 180, p = cam.pitch * Math.PI / 180;
  return [cam.t[0] + cam.dist * Math.cos(p) * Math.sin(y),
          cam.t[1] + cam.dist * Math.sin(p),
          cam.t[2] + cam.dist * Math.cos(p) * Math.cos(y)];
}
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const norm = a => { const l = Math.hypot(...a) || 1; return [a[0] / l, a[1] / l, a[2] / l]; };
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
function basis() {
  const e = eye(), f = norm(sub(cam.t, e)), r = norm(cross(f, [0, 1, 0])), u = cross(r, f);
  return { e, f, r, u };
}
function viewProj(aspect) {
  const { e, f, r, u } = basis();
  const fov = 45 * Math.PI / 180, t = 1 / Math.tan(fov / 2);
  const n = Math.max(0.002, cam.dist * 0.02), fa = cam.dist * 4 + item.R * 6;
  const V = [r[0], u[0], -f[0], 0, r[1], u[1], -f[1], 0, r[2], u[2], -f[2], 0,
             -dot(r, e), -dot(u, e), dot(f, e), 1];
  const P = [t / aspect, 0, 0, 0, 0, t, 0, 0, 0, 0, (fa + n) / (n - fa), -1, 0, 0, 2 * fa * n / (n - fa), 0];
  const M = new Float32Array(16);
  for (let c = 0; c < 4; c++) for (let r2 = 0; r2 < 4; r2++) {
    let s = 0; for (let k = 0; k < 4; k++) s += P[k * 4 + r2] * V[c * 4 + k];
    M[c * 4 + r2] = s;
  }
  return M;
}

// ---- 입력 ----
let drag = null;
canvas.addEventListener('contextmenu', e => e.preventDefault());
canvas.addEventListener('pointerdown', e => {
  canvas.setPointerCapture(e.pointerId);
  drag = { x: e.clientX, y: e.clientY, pan: e.button !== 0 || e.shiftKey };
});
canvas.addEventListener('pointerup', () => { drag = null; });
canvas.addEventListener('pointermove', e => {
  if (!drag) return;
  const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
  drag.x = e.clientX; drag.y = e.clientY;
  if (drag.pan) {
    const { r, u } = basis(), k = cam.dist * 0.0016;
    for (let i = 0; i < 3; i++) cam.t[i] += (-r[i] * dx + u[i] * dy) * k;
  } else {
    cam.yaw -= dx * 0.35;
    cam.pitch = Math.max(-89.5, Math.min(89.5, cam.pitch + dy * 0.35));
  }
});
canvas.addEventListener('wheel', e => {
  e.preventDefault();
  cam.dist = Math.max(item.R * 0.15, Math.min(item.R * 12, cam.dist * Math.exp(e.deltaY * 0.0012)));
}, { passive: false });
document.querySelectorAll('button[data-view]').forEach(b =>
  b.addEventListener('click', () => { cam = view(b.dataset.view); }));
const $ = id => document.getElementById(id);

function frame() {
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  const w = Math.round(canvas.clientWidth * dpr), h = Math.round(canvas.clientHeight * dpr);
  if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
  gl.viewport(0, 0, w, h);
  gl.clearColor(0.86, 0.94, 0.90, 1);
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  gl.enable(gl.DEPTH_TEST);
  if ($('cull').checked) gl.enable(gl.CULL_FACE); else gl.disable(gl.CULL_FACE);
  gl.uniformMatrix4fv(uVP, false, viewProj(w / h));
  gl.uniform1f(uWire, $('wire').checked ? 1 : 0);
  gl.uniform1f(uFlat, 0);
  gl.uniform1f(uUseTex, item.tex && $('tex').checked ? 1 : 0);
  if (item.tex) gl.bindTexture(gl.TEXTURE_2D, item.tex);
  draw(item.bufs, item.count, gl.TRIANGLES);
  gl.uniform1f(uUseTex, 0);
  if ($('axes').checked) {
    gl.uniform1f(uFlat, 1);
    gl.disable(gl.DEPTH_TEST);                         // 모델에 가려도 원점이 보이게
    draw(item.axes, 6, gl.LINES);
  }
  const e = eye();
  $('info').textContent =
    `${item.label} (${item.name}.obj) · ${item.count / 3} 삼각형 · ` +
    `크기 ${item.size.map(v => v.toFixed(2)).join(' × ')} | ` +
    `카메라 (${e.map(v => v.toFixed(1)).join(', ')}) → (${cam.t.map(v => v.toFixed(1)).join(', ')}) | ` +
    `축: 빨강 X · 초록 Y · 파랑 Z | 왼쪽 드래그 회전 · 오른쪽 드래그(또는 Shift) 이동 · 휠 확대`;
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
</script>
</body>
</html>
"""


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    folder = os.path.join(root, 'Assets', 'Models')
    models = {'list': []}
    for name, label in MODEL_LIST:
        if not os.path.exists(os.path.join(folder, name + '.obj')):
            continue
        models['list'].append([name, label])
        for ext in ('obj', 'mtl'):
            with open(os.path.join(folder, '%s.%s' % (name, ext)), encoding='ascii') as f:
                models['%s_%s' % (name, ext)] = f.read()
        for line in models[name + '_mtl'].splitlines():
            if line.startswith('map_Kd '):
                with open(os.path.join(folder, line.split(None, 1)[1].strip()), 'rb') as f:
                    models[name + '_tex'] = 'data:image/png;base64,' + base64.b64encode(f.read()).decode('ascii')
    out = os.path.join(folder, 'viewer.html')
    with open(out, 'w', encoding='utf-8', newline='\n') as f:
        f.write(HTML.replace('__MODELS__', json.dumps(models).replace('</', '<\\/')))
    print('saved', out, os.path.getsize(out), 'bytes,', len(models['list']), 'models')


if __name__ == '__main__':
    main()
