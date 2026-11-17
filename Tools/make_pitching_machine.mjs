// 투구 기계(피칭 머신) OBJ 생성 스크립트 (Node.js, 외부 패키지 없음)
//
// 실행:  node Tools/make_pitching_machine.mjs
// 출력:  Assets/Models/pitching_machine.obj, pitching_machine.mtl
//
// 모델 좌표 (선수 인형과 같은 규칙):
//   원점 = 기계 바닥 가운데, +Y = 위, +Z = 공이 나가는 쪽(홈 방향)
//   1 단위 = 3.048m. 공은 선수 머리 크기(반지름 0.094)라 그 공이 지나갈 수 있게 만든다.
//
// 생김새: 주황색 몸통 앞으로 발사 포신이 나와 있고, 양옆에 공을 밀어내는 검은 바퀴 두 개,
// 뒤쪽에는 투수가 조작하는 상하좌우 버튼판,
// 뒤쪽 위로 공이 줄지어 들어 있는 급탄관과 깔때기가 있다.
//
// OBJ 형식은 다른 모델과 같다: 삼각형만, f v/vt/vn, vt 는 "0 0" 하나뿐인 자리 채움.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

// ── 치수 ────────────────────────────────────────────────────────
const BALL_R = 0.094;           // 공 반지름 (선수 머리 크기)
const BASE = { w: 0.56, d: 0.50, h: 0.05 };          // 바닥판
const BODY = { x: 0.22, z0: -0.22, z1: 0.14, y0: 0.05, y1: 0.36, bevel: 0.05 }; // 몸통
const BARREL = { y: 0.22, r: 0.125, inner: 0.105, z0: 0.10, z1: 0.38 };       // 발사 포신
const WHEEL = { x: 0.20, y: 0.22, z: 0.04, r: 0.11, h: 0.07 };                // 발사 바퀴
const TUBE = { z: -0.08, r: 0.115, y0: BODY.y1, y1: 0.60, rod: 0.012 };       // 급탄관
const HOPPER = { y0: TUBE.y1, y1: 0.74, r1: 0.24, wall: 0.018 };              // 깔때기
// 뒤쪽 조작판: 판 반폭, 판 두께, 버튼 중심 간격, 버튼 반폭, 판 중심 높이
const PANEL = { half: 0.12, depth: 0.02, gap: 0.058, key: 0.026, y: 0.205 };
const SEG = 32;                 // 원 둘레 분할 수

const MATERIALS = {
  machine_orange: { kd: [0.93, 0.50, 0.22] },
  machine_dark: { kd: [0.20, 0.22, 0.26] },
  machine_mouth: { kd: [0.05, 0.05, 0.06] },
  metal: { kd: [0.74, 0.77, 0.81] },
  tire: { kd: [0.10, 0.10, 0.11] },
  ball_white: { kd: [0.97, 0.97, 0.94] },
  button: { kd: [0.90, 0.91, 0.93] },
  button_arrow: { kd: [0.93, 0.50, 0.22] },
};

// ── 메시 누적 ───────────────────────────────────────────────────
const verts = [], vIndex = new Map(), norms = [], nIndex = new Map();
const groups = new Map(); // 재질 -> 면 목록
let cur = null;
const key = (p) => p.map((c) => (Math.abs(c) < 5e-5 ? 0 : c).toFixed(4)).join(" ");
const vid = (p) => { const k = key(p); if (!vIndex.has(k)) { verts.push(k); vIndex.set(k, verts.length); } return vIndex.get(k); };
const nid = (n) => { const k = key(n); if (!nIndex.has(k)) { norms.push(k); nIndex.set(k, norms.length); } return nIndex.get(k); };
const use = (mat) => { if (!groups.has(mat)) groups.set(mat, []); cur = groups.get(mat); };

const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const mul = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const unit = (a) => { const l = Math.hypot(...a); return l < 1e-12 ? a : mul(a, 1 / l); };

// 정점별 법선 삼각형. 감는 순서는 법선 쪽에서 볼 때 반시계가 되게 맞춘다.
// 파일에 쓰는 소수 넷째 자리로 먼저 반올림해서, 반올림 뒤에도 방향과 넓이가 맞는지 본다.
const snap = (p) => p.map((c) => Math.round(c * 1e4) / 1e4);
function triS(a, b, c, na, nb, nc) {
  [a, b, c] = [snap(a), snap(b), snap(c)];
  const g = cross(sub(b, a), sub(c, a));
  if (Math.hypot(...g) < 1e-7) return; // 모서리가 한 점으로 모이며 생기는 아주 작은 조각은 버린다
  if (dot(g, add(add(na, nb), nc)) < 0) { [b, c] = [c, b]; [nb, nc] = [nc, nb]; }
  cur.push([[vid(a), nid(unit(na))], [vid(b), nid(unit(nb))], [vid(c), nid(unit(nc))]]);
}
const quadS = (a, b, c, d, na, nb, nc, nd) => { triS(a, b, c, na, nb, nc); triS(a, c, d, na, nc, nd); };
// 평면 다각형 (법선은 hint 쪽)
function poly(P, hint) {
  let n = [0, 0, 0];
  for (let i = 0; i < P.length; i++) n = add(n, cross(P[i], P[(i + 1) % P.length]));
  n = unit(dot(n, hint) < 0 ? mul(n, -1) : n);
  for (let i = 1; i < P.length - 1; i++) triS(P[0], P[i], P[i + 1], n, n, n);
}

// 회전체: 중심 c, 축 방향 axis 둘레로 단면 [[r, h], ...] 를 돌린다.
// 단면을 따라갈 때 오른쪽이 바깥(법선 방향)이다. smooth: 단면 방향으로도 법선을 평균한다.
function revolve(c, axis, prof, { segs = SEG, smooth = true } = {}) {
  const ax = unit(axis);
  const b1 = unit(cross(ax, Math.abs(ax[1]) < 0.9 ? [0, 1, 0] : [1, 0, 0])), b2 = cross(ax, b1);
  const radial = (t) => add(mul(b1, Math.cos(t)), mul(b2, Math.sin(t)));
  const segN = [];
  for (let i = 0; i < prof.length - 1; i++) {
    const dr = prof[i + 1][0] - prof[i][0], dh = prof[i + 1][1] - prof[i][1], l = Math.hypot(dr, dh);
    segN.push([dh / l, -dr / l]);
  }
  const vertN = prof.map((_, j) => {
    const a = segN[Math.max(0, j - 1)], b = segN[Math.min(segN.length - 1, j)];
    return [a[0] + b[0], a[1] + b[1]];
  });
  const P = ([r, h], t) => add(c, add(mul(radial(t), r), mul(ax, h)));
  const N = ([nr, nh], t) => add(mul(radial(t), nr), mul(ax, nh));
  for (let i = 0; i < prof.length - 1; i++) {
    const n0 = smooth ? vertN[i] : segN[i], n1 = smooth ? vertN[i + 1] : segN[i];
    for (let k = 0; k < segs; k++) {
      const a = (2 * Math.PI * k) / segs, b = (2 * Math.PI * (k + 1)) / segs;
      quadS(P(prof[i], a), P(prof[i + 1], a), P(prof[i + 1], b), P(prof[i], b), N(n0, a), N(n1, a), N(n1, b), N(n0, b));
    }
  }
}
const arc = (rc, hc, rho, a0, a1, n) => Array.from({ length: n + 1 }, (_, k) => {
  const a = a0 + ((a1 - a0) * k) / n;
  return [rc + rho * Math.cos(a), hc + rho * Math.sin(a)];
});
// 구: 아래 극에서 위 극까지
const sphere = (c, r, rings = 16) => revolve(c, [0, 1, 0], arc(0, 0, r, -Math.PI / 2, Math.PI / 2, rings).map(([x, y]) => [Math.max(x, 0), y]));
// 원통 (양 끝 막음), 모서리를 둥글게 깎을 수 있다
function cylinder(c, axis, r, h0, h1, round = 0) {
  if (round <= 0) return revolve(c, axis, [[0, h0], [r, h0], [r, h1], [0, h1]], { smooth: false });
  revolve(c, axis, [[0, h0], ...arc(r - round, h0 + round, round, -Math.PI / 2, 0, 4), ...arc(r - round, h1 - round, round, 0, Math.PI / 2, 4), [0, h1]]);
}
// 원환 (튜브 반지름 rho, 중심 반지름 R)
const torus = (c, axis, R, rho, n = 12) => revolve(c, axis, arc(R, 0, rho, -Math.PI, Math.PI, n));
// 두 점을 잇는 원기둥 막대
function rod(p0, p1, r) {
  const ax = sub(p1, p0);
  revolve(p0, ax, [[0, 0], [r, 0], [r, Math.hypot(...ax)], [0, Math.hypot(...ax)]], { segs: 12, smooth: false });
}
// 모서리를 둥글게 깎은 상자. 단면(가로 x, 깊이 z)의 둥근 사각형을 위로 쌓고 윗모서리도 둥글게 한다.
function roundedBox(cx, cz, hx, hz, y0, y1, r, n = 6) {
  const outline = (inset) => {
    const pts = [];
    for (const [sx, sz, t0] of [[1, 1, 0], [-1, 1, 90], [-1, -1, 180], [1, -1, 270]])
      for (let i = 0; i <= n; i++) {
        const t = ((t0 + (90 * i) / n) * Math.PI) / 180, rr = r - inset;
        pts.push([cx + sx * (hx - r) + rr * Math.cos(t), cz + sz * (hz - r) + rr * Math.sin(t), Math.cos(t), Math.sin(t)]);
      }
    return pts;
  };
  // 단면 링: (안쪽으로 들어간 거리, 높이, 법선의 수평/수직 성분)
  const rings = [[0, y0, 1, 0], [0, y1 - r, 1, 0]];
  for (let k = 1; k <= n; k++) { const a = ((k / n) * Math.PI) / 2; rings.push([r * (1 - Math.cos(a)), y1 - r + r * Math.sin(a), Math.cos(a), Math.sin(a)]); }
  for (let k = 0; k < rings.length - 1; k++) {
    const A = outline(Math.min(rings[k][0], r - 1e-4)), B = outline(Math.min(rings[k + 1][0], r - 1e-4));
    for (let i = 0; i < A.length; i++) {
      const j = (i + 1) % A.length;
      const p = (Q, ring) => [Q[0], ring[1], Q[1]];
      const nn = (Q, ring) => [Q[2] * ring[2], ring[3], Q[3] * ring[2]];
      quadS(p(A[i], rings[k]), p(A[j], rings[k]), p(B[j], rings[k + 1]), p(B[i], rings[k + 1]),
        nn(A[i], rings[k]), nn(A[j], rings[k]), nn(B[j], rings[k + 1]), nn(B[i], rings[k + 1]));
    }
  }
  poly(outline(r - 1e-4).map((Q) => [Q[0], y1, Q[1]]), [0, 1, 0]);
}

// ════════════════════════════════════════════════════════════════
// 바닥판
use("machine_dark");
roundedBox(0, 0, BASE.w / 2, BASE.d / 2, 0, BASE.h, 0.03, 4);

// 몸통
use("machine_orange");
roundedBox(0, (BODY.z0 + BODY.z1) / 2, BODY.x, (BODY.z1 - BODY.z0) / 2, BODY.y0, BODY.y1, BODY.bevel);

// 발사 포신: 바깥 원통 + 앞쪽 금속 테 + 안쪽 어두운 구멍
use("machine_orange");
revolve([0, BARREL.y, 0], [0, 0, 1], [[BARREL.r, BARREL.z0], [BARREL.r, BARREL.z1 - 0.03]]);
use("metal");
revolve([0, BARREL.y, 0], [0, 0, 1], [[BARREL.r, BARREL.z1 - 0.03], ...arc(BARREL.r - 0.004, BARREL.z1 - 0.012, 0.016, -Math.PI / 2, Math.PI / 2, 6), [BARREL.inner, BARREL.z1]]);
use("machine_mouth");
revolve([0, BARREL.y, 0], [0, 0, 1], [[BARREL.inner, BARREL.z1], [BARREL.inner, BARREL.z0 + 0.02], [0, BARREL.z0 + 0.02]], { smooth: false });

// 발사 바퀴 두 개: 세로축 타이어 + 금속 허브 캡
for (const s of [-1, 1]) {
  const c = [s * WHEEL.x, WHEEL.y, WHEEL.z];
  use("tire");
  cylinder(c, [0, 1, 0], WHEEL.r, -WHEEL.h / 2, WHEEL.h / 2, 0.02);
  use("metal");
  cylinder(c, [0, 1, 0], WHEEL.r * 0.45, -WHEEL.h / 2 - 0.006, WHEEL.h / 2 + 0.006, 0.006);
  // 바퀴 덮개 (위쪽 반원 띠)
  use("machine_dark");
  const R = WHEEL.r + 0.02;
  // 바깥(±X) 쪽 반원만 덮는다
  const P = (t, y) => [c[0] + s * R * Math.cos(t), y, c[2] + R * Math.sin(t)];
  const N = (t) => [s * Math.cos(t), 0, Math.sin(t)];
  const yb = WHEEL.y - WHEEL.h / 2 - 0.01, yt = WHEEL.y + WHEEL.h / 2 + 0.01;
  for (let k = 0; k < 16; k++) {
    const ta = (Math.PI * k) / 16 - Math.PI / 2, tb = (Math.PI * (k + 1)) / 16 - Math.PI / 2;
    quadS(P(ta, yb), P(tb, yb), P(tb, yt), P(ta, yt), N(ta), N(tb), N(tb), N(ta));
  }
}

// 급탄관: 금속 고리 3개와 세로 막대 4개, 안에 공 하나
use("metal");
const tubeC = (y) => [0, y, TUBE.z];
for (const y of [TUBE.y0 + 0.01, (TUBE.y0 + TUBE.y1) / 2, TUBE.y1]) torus(tubeC(y), [0, 1, 0], TUBE.r, 0.014);
for (let k = 0; k < 4; k++) {
  const t = Math.PI / 4 + (Math.PI / 2) * k, x = TUBE.r * Math.cos(t), z = TUBE.z + TUBE.r * Math.sin(t);
  rod([x, TUBE.y0, z], [x, TUBE.y1, z], TUBE.rod);
}
use("ball_white");
for (let i = 0; i < 1; i++) sphere(tubeC(TUBE.y0 + BALL_R + 0.005 + i * (2 * BALL_R + 0.01)), BALL_R);

// 깔때기: 두께 있는 그릇 모양 (바깥면 → 테두리 → 안쪽면), 안에 공 하나
use("machine_orange");
{
  const { y0, y1, r1, wall } = HOPPER, r0 = TUBE.r + 0.01;
  const outer = Array.from({ length: 9 }, (_, i) => { const u = i / 8; return [r0 + (r1 - r0) * Math.sin((u * Math.PI) / 2), y0 + (y1 - y0) * u]; });
  const inner = outer.slice().reverse().map(([r, h]) => [r - wall, h]);
  revolve([0, 0, TUBE.z], [0, 1, 0], [...outer, ...arc(r1 - wall / 2, y1, wall / 2, 0, Math.PI, 4).slice(1, -1), ...inner]);
}
use("ball_white");
sphere(tubeC(HOPPER.y0 + 0.06), BALL_R);

// 뒤쪽 조작판: 투수가 기계 뒤(-Z)에 서서 상하좌우 버튼을 누른다.
// 판 좌표: u = 조작하는 사람 기준 오른쪽(-X), v = 위(+Y), n = 사람 쪽(-Z)
{
  const U = [-1, 0, 0], Vv = [0, 1, 0], Nn = [0, 0, -1];
  const center = [0, PANEL.y, BODY.z0];
  const at = (u, v, d) => add(center, add(add(mul(U, u), mul(Vv, v)), mul(Nn, d)));
  // 평면 윤곽(u, v)을 판에서 d0 → d1 만큼 튀어나오게 한 각기둥 (뒤판은 생략)
  const prism = (outline, d0, d1) => {
    const c = outline.reduce((s, [u, v]) => [s[0] + u / outline.length, s[1] + v / outline.length], [0, 0]);
    for (let i = 0; i < outline.length; i++) {
      const [u0, v0] = outline[i], [u1, v1] = outline[(i + 1) % outline.length];
      const out = add(mul(U, (u0 + u1) / 2 - c[0]), mul(Vv, (v0 + v1) / 2 - c[1]));
      poly([at(u0, v0, d0), at(u1, v1, d0), at(u1, v1, d1), at(u0, v0, d1)], out);
    }
    poly(outline.map(([u, v]) => at(u, v, d1)), Nn);
  };
  const roundRect = (cu, cv, hu, hv, r, n = 4) => {
    const pts = [];
    for (const [su, sv, t0] of [[1, 1, 0], [-1, 1, 90], [-1, -1, 180], [1, -1, 270]])
      for (let i = 0; i <= n; i++) { const t = ((t0 + (90 * i) / n) * Math.PI) / 180; pts.push([cu + su * (hu - r) + r * Math.cos(t), cv + sv * (hv - r) + r * Math.sin(t)]); }
    return pts;
  };
  // 판
  use("machine_dark");
  prism(roundRect(0, 0, PANEL.half, PANEL.half, 0.025, 6), 0, PANEL.depth);
  // 가운데 금속 원판 (버튼 네 개를 잇는 십자 받침)
  use("metal");
  prism(Array.from({ length: 24 }, (_, i) => [0.022 * Math.cos((2 * Math.PI * i) / 24), 0.022 * Math.sin((2 * Math.PI * i) / 24)]), PANEL.depth, PANEL.depth + 0.006);
  // 상하좌우 버튼: 둥근 사각 키 + 그 위로 튀어나온 화살표
  for (const [du, dv] of [[0, 1], [0, -1], [-1, 0], [1, 0]]) {
    const ku = du * PANEL.gap, kv = dv * PANEL.gap, k = PANEL.key;
    use("button");
    prism(roundRect(ku, kv, k, k, 0.012), PANEL.depth, PANEL.depth + 0.018);
    const pu = -dv, pv = du; // 화살표 방향에 수직
    use("button_arrow");
    prism([
      [ku + du * 0.022, kv + dv * 0.022],
      [ku - du * 0.012 + pu * 0.018, kv - dv * 0.012 + pv * 0.018],
      [ku - du * 0.012 - pu * 0.018, kv - dv * 0.012 - pv * 0.018],
    ], PANEL.depth + 0.018, PANEL.depth + 0.026);
  }
}

// 몸통 옆 장식 띠 (금속)
use("metal");
for (const s of [-1, 1]) roundedBox(s * (BODY.x + 0.004), -0.12, 0.006, 0.07, 0.29, 0.31, 0.003, 2);

// ── 파일 쓰기 ───────────────────────────────────────────────────
const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const outDir = join(root, "Assets", "Models");
mkdirSync(outDir, { recursive: true });
const triCount = [...groups.values()].reduce((s, g) => s + g.length, 0);
let obj = "# Pitching machine (origin = bottom center, launches toward +Z)\n";
obj += "# generated by Tools/make_pitching_machine.mjs\n";
obj += `# triangles: ${triCount}, vertices: ${verts.length}\n`;
obj += `# barrel mouth center (0, ${BARREL.y}, ${BARREL.z1}), ball radius ${BALL_R}\n`;
obj += "mtllib pitching_machine.mtl\n";
obj += verts.map((v) => `v ${v}`).join("\n") + "\n";
obj += "vt 0.00000 0.00000\n";
obj += norms.map((n) => `vn ${n}`).join("\n") + "\n";
for (const [mat, faces] of groups) {
  obj += `usemtl ${mat}\n`;
  obj += faces.map((t) => "f " + t.map(([v, n]) => `${v}/1/${n}`).join(" ")).join("\n") + "\n";
}
let mtl = "# materials for pitching_machine.obj\n";
const f3 = (a) => a.map((x) => x.toFixed(3)).join(" ");
for (const mat of groups.keys()) {
  const m = MATERIALS[mat];
  mtl += `\nnewmtl ${mat}\nKa ${f3(m.kd)}\nKd ${f3(m.kd)}\nKs 0.050 0.050 0.050\nNs 8.0\n`;
  if (m.ke) mtl += `Ke ${f3(m.ke)}\n`;
  mtl += "d 1.0\nillum 2\n";
}
writeFileSync(join(outDir, "pitching_machine.obj"), obj);
writeFileSync(join(outDir, "pitching_machine.mtl"), mtl);
console.log(`pitching_machine.obj: ${triCount} triangles, ${verts.length} vertices, ${norms.length} normals`);
for (const [mat, faces] of groups) console.log(`  ${mat.padEnd(15)} ${String(faces.length).padStart(5)} tris`);
