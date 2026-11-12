# -*- coding: utf-8 -*-
"""
OBJ 모델을 품은 단독 실행 3D 뷰어(HTML)를 만든다. 외부 라이브러리, 서버 불필요.

실행:  python Tools/make_viewer.py
출력:  Assets/Models/viewer.html   (더블클릭하면 브라우저에서 열림)

모델을 다시 생성했으면 이 스크립트도 다시 실행해야 뷰어에 반영된다.
"""
import json
import os

HTML = r"""<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>토이 베이스볼 경기장 뷰어</title>
<style>
  html, body { margin: 0; height: 100%; overflow: hidden; background: #dcefe6;
               font: 14px/1.4 "Malgun Gothic", "Segoe UI", sans-serif; color: #17303a; }
  canvas { display: block; width: 100%; height: 100%; cursor: grab; touch-action: none; }
  canvas:active { cursor: grabbing; }
  #bar { position: fixed; left: 12px; top: 12px; display: flex; flex-wrap: wrap; gap: 6px;
         align-items: center; max-width: calc(100% - 24px); }
  button, label.chk { border: 1px solid #9fb8b0; background: #ffffffd9; border-radius: 6px;
         padding: 6px 10px; font: inherit; color: inherit; cursor: pointer; }
  button:hover { background: #fff; }
  label.chk input { vertical-align: -2px; margin: 0 4px 0 0; }
  #info { position: fixed; left: 12px; bottom: 10px; background: #ffffffc0; border-radius: 6px;
          padding: 5px 9px; font-size: 12px; }
</style>
</head>
<body>
<canvas id="c"></canvas>
<div id="bar">
  <button data-view="game">게임 시점</button>
  <button data-view="top">위</button>
  <button data-view="front">앞</button>
  <button data-view="side">옆</button>
  <button data-view="back">뒤</button>
  <label class="chk"><input type="checkbox" id="holes" checked>수비 구멍</label>
  <label class="chk"><input type="checkbox" id="move" checked>구멍 움직임</label>
  <label class="chk"><input type="checkbox" id="cull" checked>뒷면 제거</label>
  <label class="chk"><input type="checkbox" id="wire">와이어프레임</label>
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
  const V = [], pos = [], nor = [], col = [], emi = [], bary = [];
  let m = { kd: [.8, .8, .8], ke: [0, 0, 0] };
  for (const line of objText.split('\n')) {
    const s = line.trim().split(/\s+/);
    if (s[0] === 'v') V.push([+s[1], +s[2], +s[3]]);
    else if (s[0] === 'usemtl') m = mats[s[1]] || m;
    else if (s[0] === 'f') {
      const idx = s.slice(1).map(t => parseInt(t.split('/')[0]) - 1);
      for (let i = 1; i < idx.length - 1; i++) {
        const a = V[idx[0]], b = V[idx[i]], c = V[idx[i + 1]];
        const ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
        const vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
        let nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const l = Math.hypot(nx, ny, nz) || 1; nx /= l; ny /= l; nz /= l;
        [a, b, c].forEach((p, k) => {
          pos.push(p[0], p[1], p[2]); nor.push(nx, ny, nz);
          col.push(m.kd[0], m.kd[1], m.kd[2]); emi.push(m.ke[0], m.ke[1], m.ke[2]);
          bary.push(k === 0 ? 1 : 0, k === 1 ? 1 : 0, k === 2 ? 1 : 0);
        });
      }
    }
  }
  return { pos, nor, col, emi, bary, count: pos.length / 3 };
}

const canvas = document.getElementById('c');
const gl = canvas.getContext('webgl', { antialias: true });
gl.getExtension('OES_standard_derivatives');

const VS = `
attribute vec3 aPos, aNor, aCol, aEmi, aBary;
uniform mat4 uVP; uniform vec3 uOffset;
varying vec3 vNor, vCol, vEmi, vBary;
void main() {
  vNor = aNor; vCol = aCol; vEmi = aEmi; vBary = aBary;
  gl_Position = uVP * vec4(aPos + uOffset, 1.0);
}`;
const FS = `
#extension GL_OES_standard_derivatives : enable
precision mediump float;
varying vec3 vNor, vCol, vEmi, vBary;
uniform float uWire;
void main() {
  vec3 n = normalize(vNor);
  if (!gl_FrontFacing) n = -n;
  float d = max(dot(n, normalize(vec3(0.35, 0.85, 0.40))), 0.0);
  vec3 c = min(vCol * (0.42 + 0.58 * d) + vEmi * 0.5, 1.0);
  if (uWire > 0.5) {
    vec3 w = fwidth(vBary);
    vec3 e = smoothstep(vec3(0.0), w * 1.2, vBary);
    c = mix(vec3(0.05, 0.12, 0.15), c, min(min(e.x, e.y), e.z));
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
const uOffset = gl.getUniformLocation(prog, 'uOffset');
const uWire = gl.getUniformLocation(prog, 'uWire');

function upload(mesh) {
  const bufs = {};
  for (const k of ['pos', 'nor', 'col', 'emi', 'bary']) {
    bufs[k] = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, bufs[k]);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(mesh[k]), gl.STATIC_DRAW);
  }
  return { bufs, count: mesh.count };
}
const ATTR = { pos: 'aPos', nor: 'aNor', col: 'aCol', emi: 'aEmi', bary: 'aBary' };
function draw(obj, offset) {
  for (const k in ATTR) {
    const loc = gl.getAttribLocation(prog, ATTR[k]);
    gl.bindBuffer(gl.ARRAY_BUFFER, obj.bufs[k]);
    gl.enableVertexAttribArray(loc);
    gl.vertexAttribPointer(loc, 3, gl.FLOAT, false, 0, 0);
  }
  gl.uniform3fv(uOffset, offset);
  gl.drawArrays(gl.TRIANGLES, 0, obj.count);
}

const stadium = upload(parse(MODELS.stadium_obj, MODELS.stadium_mtl));
const hole = upload(parse(MODELS.hole_obj, MODELS.hole_mtl));
const HOLES = [[-8.5, -17.5, 3.0, 0.0], [0, -21.5, 3.5, 2.1], [8.5, -17.5, 3.0, 4.2]];  // x, z, 이동폭, 위상

// ---- 카메라 (타깃 중심 궤도) ----
const VIEWS = {
  game:  { t: [0, 0, -14], yaw: 0,   pitch: 39.5, dist: 51.9 },
  top:   { t: [0, 0, -20], yaw: 0,   pitch: 89.0, dist: 75 },
  front: { t: [0, 2, -20], yaw: 0,   pitch: 8,    dist: 62 },
  side:  { t: [0, 2, -20], yaw: 90,  pitch: 18,   dist: 66 },
  back:  { t: [0, 2, -20], yaw: 180, pitch: 22,   dist: 66 },
};
let cam = JSON.parse(JSON.stringify(VIEWS.game));
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
  const fov = 45 * Math.PI / 180, n = 0.5, fa = 600, t = 1 / Math.tan(fov / 2);
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
    cam.pitch = Math.max(-10,Math.min(89.5, cam.pitch + dy * 0.35));
  }
});
canvas.addEventListener('wheel', e => {
  e.preventDefault();
  cam.dist = Math.max(3, Math.min(250, cam.dist * Math.exp(e.deltaY * 0.0012)));
}, { passive: false });
document.querySelectorAll('button[data-view]').forEach(b =>
  b.addEventListener('click', () => { cam = JSON.parse(JSON.stringify(VIEWS[b.dataset.view])); }));
const $ = id => document.getElementById(id);

function frame(ms) {
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
  draw(stadium, [0, 0, 0]);
  if ($('holes').checked) {
    const t = $('move').checked ? ms / 1000 : 0;
    for (const [x, z, amp, ph] of HOLES) draw(hole, [x + amp * Math.sin(t * 1.3 + ph), 0.03, z]);
  }
  const e = eye();
  $('info').textContent =
    `경기장 ${stadium.count / 3} 삼각형 · 구멍 ${hole.count / 3} 삼각형 | ` +
    `카메라 (${e.map(v => v.toFixed(1)).join(', ')}) → (${cam.t.map(v => v.toFixed(1)).join(', ')}) | ` +
    `왼쪽 드래그 회전 · 오른쪽 드래그(또는 Shift) 이동 · 휠 확대`;
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
    models = {}
    for name in ('stadium', 'hole'):
        for ext in ('obj', 'mtl'):
            with open(os.path.join(folder, '%s.%s' % (name, ext)), encoding='ascii') as f:
                models['%s_%s' % (name, ext)] = f.read()
    out = os.path.join(folder, 'viewer.html')
    with open(out, 'w', encoding='utf-8', newline='\n') as f:
        f.write(HTML.replace('__MODELS__', json.dumps(models).replace('</', '<\\/')))
    print('saved', out, os.path.getsize(out), 'bytes')


if __name__ == '__main__':
    main()
