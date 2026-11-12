# -*- coding: utf-8 -*-
"""
토이 베이스볼 경기장 OBJ 생성 스크립트 (블렌더 불필요, 파이썬 + numpy)

실행:  python Tools/make_stadium.py
출력:  Assets/Models/stadium.obj, stadium.mtl   (고정 경기장)
       Assets/Models/hole.obj,    hole.mtl      (움직이는 수비 구멍 1개, 원점 기준)

월드 좌표계 (OpenGL 기본, 오른손):
  원점 = 홈플레이트, +Y = 위, -Z = 2루(외야) 방향, +X = 1루 쪽(화면 오른쪽)
  카메라는 +Z 쪽 위에서 -Z 를 내려다보는 고정 사선 시점을 가정한다.

제작용 로컬 좌표 (a, y, b):
  a = 1루 파울라인 방향, b = 3루 파울라인 방향. 경기장이 a, b 축에 정렬되어
  만들기 쉽고, 마지막에 45도 돌려 월드 좌표로 내보낸다.
      x = (a - b) / sqrt2,   z = -(a + b) / sqrt2
"""
import math
import os
import numpy as np

S2 = math.sqrt(2.0)

# ----------------------------------------------------------------------------
# 조절 가능한 주요 치수
# ----------------------------------------------------------------------------
BASE_DIST = 9.0        # 베이스 간 거리
FENCE_R = 27.0         # 홈에서 외야 펜스 안쪽까지
FENCE_T = 0.5          # 펜스 두께
FENCE_H = 1.7          # 펜스 높이 (이걸 넘기면 홈런)
MOUND_D = 6.05         # 홈에서 마운드까지 (2루 방향)
BOARD_MIN, BOARD_MAX = -4.0, FENCE_R + 6.0   # 판 안쪽 바닥 범위 (a, b 공통)
ARC_SEGS = 27          # 외야 원호 분할 수 (ZONES 칸 수의 배수)
ROUND_SEGS = 8         # 판 모서리 라운드 분할 수
DISC_SEGS = 16
# 펜스 안쪽 색 패널: 1=안타, 2=2루타, 3=3루타 (왼쪽 파울라인 -> 오른쪽, 10도씩 9칸)
ZONES = [1, 2, 1, 3, 2, 3, 1, 2, 1]
DUMMY_VT = True        # True: f v/vt/vn,  False: f v//vn

MATERIALS = {            # 이름: (R, G, B)  /  발광은 EMISSIVE 에 추가
    'plinth':      (0.17, 0.19, 0.22),
    'body':        (0.11, 0.31, 0.39),
    'trim':        (0.95, 0.90, 0.78),
    'foul':        (0.18, 0.47, 0.31),
    'grass_a':     (0.36, 0.73, 0.37),
    'grass_b':     (0.29, 0.65, 0.33),
    'dirt':        (0.79, 0.55, 0.35),
    'line':        (0.97, 0.97, 0.95),
    'zone_1':      (0.96, 0.77, 0.26),
    'zone_2':      (0.94, 0.54, 0.24),
    'zone_3':      (0.89, 0.31, 0.29),
    'concrete':    (0.73, 0.75, 0.78),
    'seat_a':      (0.91, 0.39, 0.24),
    'seat_b':      (0.25, 0.50, 0.77),
    'roof':        (0.91, 0.93, 0.95),
    'board_frame': (0.17, 0.19, 0.22),
    'board_face':  (0.07, 0.13, 0.16),
    'lamp_green':  (0.35, 0.85, 0.40),
    'metal':       (0.54, 0.58, 0.62),
    'bulb':        (1.00, 0.96, 0.78),
    'net':         (0.81, 0.85, 0.87),
    'bench':       (0.66, 0.44, 0.25),
    'hole_dark':   (0.06, 0.09, 0.11),
    'hole_rim':    (0.91, 0.93, 0.95),
}
EMISSIVE = {'bulb': (0.9, 0.85, 0.6)}
TEXTURES = {}           # 재질 이름: 텍스처 파일 (map_Kd). 소품 스크립트에서 채운다


# ----------------------------------------------------------------------------
# 메시 컨테이너
# ----------------------------------------------------------------------------
class Mesh:
    def __init__(self, to_world):
        self.G = np.asarray(to_world, float)   # 3x3: 로컬 (a,y,b) -> 월드 (x,y,z)
        self.tris = {}                         # 재질 -> [(p0,p1,p2), ...] 월드 좌표

    def face(self, mat, pts, away=None, toward=None, normal=None, smooth=None, uv=None):
        """볼록 다각형 한 장. away: 이 점에서 멀어지는 쪽이 앞면,
        toward: 이 점을 향하는 쪽이 앞면, normal: 이 방향이 앞면,
        셋 다 없으면 +Y 가 앞면. smooth: 꼭짓점별 법선(부드러운 음영용),
        uv: 꼭짓점별 텍스처 좌표."""
        P = [self.G @ np.asarray(p, float) for p in pts]
        N = [self.G @ np.asarray(q, float) for q in smooth] if smooth is not None else None
        U = list(uv) if uv is not None else None
        n = np.zeros(3)
        for i in range(len(P)):
            p, q = P[i], P[(i + 1) % len(P)]
            n += np.cross(p, q)
        c = sum(P) / len(P)
        if away is not None:
            want = c - self.G @ np.asarray(away, float)
        elif toward is not None:
            want = self.G @ np.asarray(toward, float) - c
        elif normal is not None:
            want = self.G @ np.asarray(normal, float)
        else:
            want = np.array([0.0, 1.0, 0.0])
        if np.dot(n, want) < 0:
            P.reverse()
            if N:
                N.reverse()
            if U:
                U.reverse()
        for i in range(1, len(P) - 1):
            a, b, c2 = P[0], P[i], P[i + 1]
            if np.linalg.norm(np.cross(b - a, c2 - a)) > 1e-9:
                self.tris.setdefault(mat, []).append((
                    (a, b, c2),
                    (N[0], N[i], N[i + 1]) if N else None,
                    (U[0], U[i], U[i + 1]) if U else None))

    def count(self):
        return sum(len(t) for t in self.tris.values())

    def write(self, obj_path, mtl_name, title):
        verts, vidx, norms, nidx = [], {}, [], {}
        uvs, uidx = ([(0.0, 0.0)], {}) if DUMMY_VT else ([], {})

        def vid(p):
            k = tuple(round(float(x), 4) + 0.0 for x in p)
            if k not in vidx:
                vidx[k] = len(verts) + 1
                verts.append(k)
            return vidx[k]

        def nid(n):
            k = tuple(round(float(x), 4) + 0.0 for x in n)
            if k not in nidx:
                nidx[k] = len(norms) + 1
                norms.append(k)
            return nidx[k]

        def uid(q):
            k = (round(float(q[0]), 5), round(float(q[1]), 5))
            if k not in uidx:
                uidx[k] = len(uvs) + 1
                uvs.append(k)
            return uidx[k]

        groups = []
        for mat in MATERIALS:
            if mat not in self.tris:
                continue
            faces = []
            for (a, b, c), tn, tuv in self.tris[mat]:
                if tn is not None:
                    ns = [nid(q / np.linalg.norm(q)) for q in tn]
                else:
                    n = np.cross(b - a, c - a)
                    ns = [nid(n / np.linalg.norm(n))] * 3
                if tuv is not None:
                    ts = [uid(q) for q in tuv]
                else:
                    ts = [1, 1, 1] if DUMMY_VT else None
                faces.append(((vid(a), vid(b), vid(c)), ns, ts))
            groups.append((mat, faces))

        with open(obj_path, 'w', encoding='ascii', newline='\n') as f:
            f.write('# %s\n' % title)
            f.write('# generated by Tools/make_stadium.py\n')
            f.write('# triangles: %d, vertices: %d\n' % (self.count(), len(verts)))
            f.write('# origin = home plate, +Y up, -Z toward 2nd base, +X toward 1st base side\n')
            f.write('mtllib %s\n' % mtl_name)
            for v in verts:
                f.write('v %.4f %.4f %.4f\n' % v)
            for q in uvs:
                f.write('vt %.5f %.5f\n' % q)
            for n in norms:
                f.write('vn %.4f %.4f %.4f\n' % n)
            for mat, faces in groups:
                f.write('g %s\nusemtl %s\n' % (mat, mat))
                for vs, ns, ts in faces:
                    if ts is not None:
                        f.write('f ' + ' '.join('%d/%d/%d' % (vs[k], ts[k], ns[k]) for k in range(3)) + '\n')
                    else:
                        f.write('f ' + ' '.join('%d//%d' % (vs[k], ns[k]) for k in range(3)) + '\n')

        with open(os.path.join(os.path.dirname(obj_path), mtl_name), 'w',
                  encoding='ascii', newline='\n') as f:
            f.write('# materials for %s\n' % os.path.basename(obj_path))
            for mat, _ in groups:
                r, g, b = (1.0, 1.0, 1.0) if mat in TEXTURES else MATERIALS[mat]
                f.write('\nnewmtl %s\n' % mat)
                f.write('Ka %.3f %.3f %.3f\n' % (r, g, b))
                f.write('Kd %.3f %.3f %.3f\n' % (r, g, b))
                f.write('Ks 0.050 0.050 0.050\nNs 8.0\n')
                if mat in EMISSIVE:
                    f.write('Ke %.3f %.3f %.3f\n' % EMISSIVE[mat])
                if mat in TEXTURES:
                    f.write('map_Kd %s\n' % TEXTURES[mat])
                f.write('d 1.0\nillum 2\n')


# ----------------------------------------------------------------------------
# 좌표 프레임: (u, h, v) -> 로컬 (a, y, b).  u = 오른쪽, h = 위, v = 앞(깊이)
# yaw = 0 이면 u = +a, v = +b
# ----------------------------------------------------------------------------
def frame(a=0.0, b=0.0, yaw=0.0, y=0.0, pitch=0.0, parent=None, swap=False):
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    R_yaw = np.array([[cy, 0, -sy, 0], [0, 1, 0, 0], [sy, 0, cy, 0], [0, 0, 0, 1.0]])
    R_pit = np.array([[1, 0, 0, 0], [0, cp, -sp, 0], [0, sp, cp, 0], [0, 0, 0, 1.0]])
    T = np.eye(4)
    T[:3, 3] = (a, y, b)
    M = T @ R_yaw @ R_pit
    if parent is not None:
        M = parent @ M
    if swap:  # a <-> b 교환 = 월드에서 좌우(x) 대칭
        S = np.array([[0, 0, 1, 0], [0, 1, 0, 0], [1, 0, 0, 0], [0, 0, 0, 1.0]])
        M = S @ M
    return M


IDENT = frame()


def yaw_facing_home(a, b):
    """앞면(v0 쪽)이 홈을 바라보도록 하는 yaw"""
    return math.degrees(math.atan2(-a, b))


def tp(M, u, h, v):
    return (M @ np.array([u, h, v, 1.0]))[:3]


# ----------------------------------------------------------------------------
# 기본 도형
# ----------------------------------------------------------------------------
def hexa(m, pts, mats, skip=''):
    """8점 육면체. pts[0:4] 아랫면, pts[4:8] 윗면(같은 순서).
    면 키: b, t, 0(점0-1 쪽), 1, 2, 3.  mats: 문자열 또는 {키: 재질, '*': 기본}"""
    c = sum(np.asarray(p, float) for p in pts) / 8.0
    faces = {'b': [0, 1, 2, 3], 't': [4, 5, 6, 7], '0': [0, 1, 5, 4],
             '1': [1, 2, 6, 5], '2': [2, 3, 7, 6], '3': [3, 0, 4, 7]}
    for k, idx in faces.items():
        if k in skip:
            continue
        mat = mats if isinstance(mats, str) else mats.get(k, mats['*'])
        m.face(mat, [pts[i] for i in idx], away=c)


def box(m, mats, M, us, hs, vs, skip='b', inset=0.0):
    """프레임 M 안의 상자. 면 키: 0 = v0(앞), 1 = u1, 2 = v1(뒤), 3 = u0.
    inset: 윗면을 안쪽으로 줄여 모서리를 깎는다."""
    (u0, u1), (h0, h1), (v0, v1) = us, hs, vs
    i = inset
    pts = [tp(M, u0, h0, v0), tp(M, u1, h0, v0), tp(M, u1, h0, v1), tp(M, u0, h0, v1),
           tp(M, u0 + i, h1, v0 + i), tp(M, u1 - i, h1, v0 + i),
           tp(M, u1 - i, h1, v1 - i), tp(M, u0 + i, h1, v1 - i)]
    hexa(m, pts, mats, skip)


def rrect(a0, b0, a1, b1, r, seg=ROUND_SEGS):
    """모서리가 둥근 사각형 윤곽 (a, b) 목록"""
    out = []
    corners = [(a1 - r, b1 - r, 0), (a0 + r, b1 - r, 90), (a0 + r, b0 + r, 180), (a1 - r, b0 + r, 270)]
    for ca, cb, t0 in corners:
        for i in range(seg + 1):
            t = math.radians(t0 + 90.0 * i / seg)
            out.append((ca + r * math.cos(t), cb + r * math.sin(t)))
    return out


def circle(ca, cb, r, seg=DISC_SEGS):
    return [(ca + r * math.cos(2 * math.pi * i / seg), cb + r * math.sin(2 * math.pi * i / seg))
            for i in range(seg)]


def at(poly, y, M=IDENT):
    return [tp(M, a, y, b) for a, b in poly]


def centroid(poly, y, M=IDENT):
    return tp(M, sum(p[0] for p in poly) / len(poly), y, sum(p[1] for p in poly) / len(poly))


def cap(m, mat, poly, y, M=IDENT):
    """볼록 윤곽의 윗면 (중심점 부채꼴)"""
    P = at(poly, y, M)
    c = centroid(poly, y, M)
    for i in range(len(P)):
        m.face(mat, [c, P[i], P[(i + 1) % len(P)]])


def loop_strip(m, mat, A, B, mode='out', c=None):
    """같은 점 수의 닫힌 윤곽 A, B 를 잇는 띠. mode: out(바깥), in(안쪽), up(위)"""
    for i in range(len(A)):
        j = (i + 1) % len(A)
        q = [A[i], A[j], B[j], B[i]]
        if mode == 'up':
            m.face(mat, q)
        elif mode == 'out':
            m.face(mat, q, away=c)
        else:
            m.face(mat, q, toward=c)


def polar(r, deg, y=0.0):
    t = math.radians(deg)
    return (r * math.cos(t), y, r * math.sin(t))


def ring_sector(m, mat, r0, r1, d0, d1, y, segs):
    for i in range(segs):
        t0 = d0 + (d1 - d0) * i / segs
        t1 = d0 + (d1 - d0) * (i + 1) / segs
        m.face(mat, [polar(r0, t0, y), polar(r1, t0, y), polar(r1, t1, y), polar(r0, t1, y)])


def plate(m, mat, M, us, vs, y):
    m.face(mat, [tp(M, us[0], y, vs[0]), tp(M, us[1], y, vs[0]),
                 tp(M, us[1], y, vs[1]), tp(M, us[0], y, vs[1])])


# ----------------------------------------------------------------------------
# 경기장 부품
# ----------------------------------------------------------------------------
def build_board(m):
    lo, hi = BOARD_MIN, BOARD_MAX
    mid = (lo + hi) / 2
    P0 = rrect(lo - 1.6, lo - 1.6, hi + 1.6, hi + 1.6, 4.4)
    P1 = rrect(lo - 1.25, lo - 1.25, hi + 1.25, hi + 1.25, 4.05)
    B0 = rrect(lo - 0.6, lo - 0.6, hi + 0.6, hi + 0.6, 3.6)
    B1 = rrect(lo - 0.48, lo - 0.48, hi + 0.48, hi + 0.48, 3.48)
    IN = rrect(lo, lo, hi, hi, 3.0)
    c = (mid, -1.0, mid)
    # 받침대
    loop_strip(m, 'plinth', at(P0, -2.1), at(P0, -1.55), 'out', c)
    loop_strip(m, 'plinth', at(P0, -1.55), at(P1, -1.2), 'out', (mid, -3.0, mid))
    loop_strip(m, 'plinth', at(P1, -1.2), at(B0, -1.2), 'up')
    # 몸통 + 장식 띠
    loop_strip(m, 'body', at(B0, -1.2), at(B0, -0.75), 'out', c)
    loop_strip(m, 'trim', at(B0, -0.75), at(B0, -0.45), 'out', c)
    loop_strip(m, 'body', at(B0, -0.45), at(B0, 0.40), 'out', c)
    loop_strip(m, 'body', at(B0, 0.40), at(B1, 0.52), 'out', (mid, -2.0, mid))
    # 테두리 윗면, 안쪽 벽, 바닥
    loop_strip(m, 'trim', at(B1, 0.52), at(IN, 0.52), 'up')
    loop_strip(m, 'body', at(IN, 0.52), at(IN, 0.0), 'in', (mid, 0.26, mid))
    cap(m, 'foul', IN, 0.0)


def build_field(m):
    # 외야 잔디 줄무늬 + 워닝 트랙
    track_r0 = FENCE_R - 1.5
    n_rings = 9
    for i in range(n_rings):
        r0 = track_r0 * i / n_rings
        r1 = track_r0 * (i + 1) / n_rings
        ring_sector(m, 'grass_a' if i % 2 == 0 else 'grass_b', r0, r1, 0, 90, 0.02, ARC_SEGS)
    ring_sector(m, 'dirt', track_r0, FENCE_R, 0, 90, 0.02, ARC_SEGS)

    # 내야 흙: 마운드 중심 원을 파울라인으로 자른 모양
    mc = MOUND_D / S2
    R = 8.6 * BASE_DIST / 9.0
    ang = math.degrees(math.atan2(-mc, math.sqrt(R * R - mc * mc)))
    pts = [(0.0, 0.04, 0.0)]
    segs = 22
    for i in range(segs + 1):
        t = math.radians(ang + (90 - 2 * ang) * i / segs)
        pts.append((mc + R * math.cos(t), 0.04, mc + R * math.sin(t)))
    c = (mc, 0.04, mc)
    for i in range(len(pts)):
        m.face('dirt', [c, pts[i], pts[(i + 1) % len(pts)]])

    # 내야 잔디
    g0, g1 = 1.3, BASE_DIST - 1.3
    cap(m, 'grass_a', rrect(g0, g0, g1, g1, 0.5, 3), 0.06)

    # 홈 주변 흙, 마운드
    cap(m, 'dirt', circle(0, 0, 2.3), 0.08)
    cap(m, 'dirt', circle(mc, mc, 1.6), 0.08)
    loop_strip(m, 'dirt', at(circle(mc, mc, 1.6), 0.08), at(circle(mc, mc, 1.0), 0.26), 'out', (mc, -1.0, mc))
    cap(m, 'dirt', circle(mc, mc, 1.0), 0.26)
    view = frame(mc, mc, yaw=-45)          # u = 화면 오른쪽, v = 2루 방향
    box(m, 'line', view, (-0.3, 0.3), (0.26, 0.31), (-0.08, 0.08))

    # 파울라인, 파울 폴
    for sw in (False, True):
        M = frame(swap=sw)
        plate(m, 'line', M, (2.3, FENCE_R), (-0.09, 0.09), 0.10)
        box(m, 'zone_1', M, (FENCE_R + 0.15, FENCE_R + 0.35), (0.0, 4.6), (-0.1, 0.1))

    # 타석 (좌우), 홈플레이트
    home = frame(0, 0, yaw=-45)
    t = 0.045
    for s in (-1, 1):
        cu, hw, hl = 1.2 * s, 0.5, 0.95
        plate(m, 'line', home, (cu - hw, cu + hw), (hl - t, hl + t), 0.10)
        plate(m, 'line', home, (cu - hw, cu + hw), (-hl - t, -hl + t), 0.10)
        plate(m, 'line', home, (cu - hw - t, cu - hw + t), (-hl - t, hl + t), 0.10)
        plate(m, 'line', home, (cu + hw - t, cu + hw + t), (-hl - t, hl + t), 0.10)
    for s in (-1, 1):                       # 대기 타석
        cap(m, 'dirt', [tp(home, 4.0 * s + 0.7 * math.cos(k * math.pi / 6), 0, 0.5 + 0.7 * math.sin(k * math.pi / 6))[[0, 2]]
                        for k in range(12)], 0.02)
    penta = [(-0.3, 0.3), (0.3, 0.3), (0.3, 0.0), (0.0, -0.3), (-0.3, 0.0)]
    m.face('line', [tp(home, u, 0.12, v) for u, v in penta])

    # 베이스 3개
    d = BASE_DIST
    for a, b in ((d, 0), (d, d), (0, d)):
        box(m, 'line', frame(a, b), (-0.42, 0.42), (0.04, 0.22), (-0.42, 0.42), inset=0.07)


def build_fence(m):
    r0, r1 = FENCE_R, FENCE_R + FENCE_T
    per = ARC_SEGS // len(ZONES)
    for i in range(ARC_SEGS):
        t0, t1 = 90.0 * i / ARC_SEGS, 90.0 * (i + 1) / ARC_SEGS
        zone = ZONES[len(ZONES) - 1 - min(i // per, len(ZONES) - 1)]  # ZONES 는 화면 왼쪽부터
        pts = [polar(r0, t0), polar(r1, t0), polar(r1, t1), polar(r0, t1),
               polar(r0, t0, FENCE_H), polar(r1, t0, FENCE_H), polar(r1, t1, FENCE_H), polar(r0, t1, FENCE_H)]
        skip = 'b'
        if i > 0:
            skip += '0'
        if i < ARC_SEGS - 1:
            skip += '2'
        hexa(m, pts, {'*': 'body', 't': 'trim', '3': 'zone_%d' % zone}, skip)


def build_stand(m, M, width, steps, depth, seat_mat, base_h=1.0, rise=0.78, blocks=3,
                wall_h=1.3, canopy=False):
    w = width / 2.0
    top = base_h + (steps - 1) * rise
    for i in range(steps):
        h = base_h + i * rise
        v0, v1 = i * depth, (i + 1) * depth
        skip = 'b2' if i < steps - 1 else 'b2'
        box(m, {'*': 'concrete', '0': 'body' if i == 0 else 'concrete'}, M, (-w, w), (0, h), (v0, v1), skip)
        # 좌석 블록 (등받이 포함), 사이는 통로
        margin, gap = 0.45, 0.8
        bw = (width - 2 * margin - (blocks - 1) * gap) / blocks
        for k in range(blocks):
            u0 = -w + margin + k * (bw + gap)
            box(m, seat_mat, M, (u0, u0 + bw), (h, h + 0.22), (v0 + 0.25, v0 + 0.80))
            box(m, seat_mat, M, (u0, u0 + bw), (h, h + 0.58), (v0 + 0.80, v0 + 0.94))
    box(m, 'trim', M, (-w, w), (base_h, base_h + 0.1), (-0.07, 0.09), skip='')   # 앞 난간
    # 뒷벽
    vb = steps * depth
    box(m, 'body', M, (-w, w), (0, top + wall_h), (vb, vb + 0.35))
    box(m, 'trim', M, (-w - 0.05, w + 0.05), (top + wall_h, top + wall_h + 0.12), (vb - 0.05, vb + 0.40))
    if canopy:
        C = frame(0, vb, y=top + wall_h + 0.12, pitch=8, parent=M)
        box(m, 'roof', C, (-w - 0.2, w + 0.2), (0, 0.16), (-3.0, 0.45), skip='')
        for u in (-w + 0.3, 0.0, w - 0.3):
            box(m, 'metal', C, (u - 0.07, u + 0.07), (-0.3, 0.0), (-2.6, 0.0))
    return top + wall_h + 0.12, vb


def build_scoreboard(m, M, top, vb):
    for u in (-3.0, 3.0):
        box(m, 'metal', M, (u - 0.18, u + 0.18), (top, top + 1.0), (vb + 0.05, vb + 0.35))
    h0 = top + 0.8
    box(m, 'board_frame', M, (-4.4, 4.4), (h0, h0 + 3.0), (vb - 0.1, vb + 0.5), skip='')
    box(m, 'board_face', M, (-4.1, 4.1), (h0 + 0.3, h0 + 2.7), (vb - 0.16, vb - 0.1), skip='b2')
    box(m, 'trim', M, (-4.5, 4.5), (h0 + 3.0, h0 + 3.15), (vb - 0.18, vb + 0.58), skip='')
    # 볼(초록 3) / 스트라이크(노랑 2) / 아웃(빨강 2) 램프 자리
    lamps = ['lamp_green'] * 3 + ['zone_1'] * 2 + ['zone_3'] * 2
    for k, mat in enumerate(lamps):
        u = -3.3 + k * 0.75 + (0.5 if k >= 3 else 0) + (0.5 if k >= 5 else 0)
        box(m, mat, M, (u - 0.2, u + 0.2), (h0 + 0.55, h0 + 0.95), (vb - 0.22, vb - 0.16), skip='b2')
    # 관중석 위 깃발
    for k, u in enumerate((-7.4, -5.7, 5.7, 7.4)):
        box(m, 'metal', M, (u - 0.04, u + 0.04), (top, top + 1.7), (vb + 0.13, vb + 0.21))
        tri = [tp(M, u + 0.04, top + 1.7, vb + 0.17), tp(M, u + 0.04, top + 1.1, vb + 0.17), tp(M, u + 1.0, top + 1.4, vb + 0.17)]
        mat = ('seat_a', 'zone_1', 'seat_b', 'seat_a')[k]
        m.face(mat, tri, away=tp(M, u, top + 1.4, vb + 5))
        m.face(mat, tri, toward=tp(M, u, top + 1.4, vb + 5))
    # 이닝 점수 칸
    for row in range(2):
        for col in range(5):
            u = -3.5 + col * 1.0
            hh = h0 + 1.3 + row * 0.65
            box(m, 'trim', M, (u, u + 0.8), (hh, hh + 0.5), (vb - 0.2, vb - 0.16), skip='b2')
    box(m, 'zone_2', M, (1.9, 3.7), (h0 + 1.3, h0 + 2.45), (vb - 0.2, vb - 0.16), skip='b2')


def build_tower(m, a, b, swap):
    M = frame(a, b, yaw=yaw_facing_home(a, b), swap=swap)
    box(m, 'board_frame', M, (-0.6, 0.6), (0, 0.6), (-0.6, 0.6), inset=0.08)
    box(m, 'metal', M, (-0.3, 0.3), (0.6, 8.6), (-0.3, 0.3), inset=0.12)
    P = frame(0, 0, y=9.3, pitch=-14, parent=M)
    box(m, 'metal', P, (-0.2, 0.2), (-1.3, -0.2), (0.1, 0.45), skip='')
    box(m, 'board_frame', P, (-2.0, 2.0), (-1.25, 1.25), (-0.18, 0.18), skip='')
    for cu in (-1.41, -0.47, 0.47, 1.41):
        for ch in (-0.75, 0.0, 0.75):
            box(m, 'bulb', P, (cu - 0.36, cu + 0.36), (ch - 0.28, ch + 0.28), (-0.27, -0.18),
                skip='2', inset=0.0)


def build_net(m, M, length, h0, h1):
    t = 0.07
    n_post = int(round(length / 3.0))
    for i in range(n_post + 1):
        u = length * i / n_post
        box(m, 'metal', M, (u - t, u + t), (0.0, h1 + 0.1), (-t, t))
    box(m, 'metal', M, (0, length), (h1 - t, h1 + t), (-t, t), skip='')
    w = 0.025
    n_v = int(round(length / 0.75))
    for i in range(1, n_v):
        if i % 4 == 0:
            continue                      # 기둥 자리
        u = length * i / n_v
        box(m, 'net', M, (u - w, u + w), (h0, h1), (-w, w), skip='bt')
    n_h = int(round((h1 - h0) / 0.7))
    for i in range(1, n_h):
        h = h0 + (h1 - h0) * i / n_h
        box(m, 'net', M, (0, length), (h - w, h + w), (-w, w), skip='13')


def build_dugout(m, M):
    box(m, 'body', M, (-3.5, 3.5), (0, 1.3), (0.0, 0.15))
    for u in (-3.5, 3.35):
        box(m, 'body', M, (u, u + 0.15), (0, 1.3), (0.15, 1.4))
    box(m, 'trim', M, (-3.7, 3.7), (1.3, 1.46), (-0.05, 1.65), skip='')
    box(m, 'bench', M, (-3.1, 3.1), (0, 0.4), (0.3, 0.8))


def build_stadium():
    G = np.array([[1 / S2, 0, -1 / S2], [0, 1, 0], [-1 / S2, 0, -1 / S2]])
    m = Mesh(G)
    build_board(m)
    build_field(m)
    build_fence(m)

    # 중앙 관중석 + 전광판 (홈을 정면으로 바라봄)
    d = (FENCE_R + FENCE_T + 1.7) / S2
    C = frame(d, d, yaw=-45)
    top, vb = build_stand(m, C, 16.0, 4, 1.4, 'seat_a', blocks=4)
    build_scoreboard(m, C, top, vb)

    hi = BOARD_MAX
    for sw in (False, True):
        # 측면 관중석 (지붕 포함)
        Sd = frame(FENCE_R + 1.6, 7.0, yaw=-90, swap=sw)
        build_stand(m, Sd, 12.0, 3, 1.25, 'seat_b', blocks=3, wall_h=2.2, canopy=True)
        # 조명탑
        build_tower(m, hi - 1.3, 14.9, sw)
        # 앞쪽 테두리의 먼 절반: 그물 펜스 / 가까운 쪽: 더그아웃
        build_net(m, frame(hi - 15.0, BOARD_MIN + 0.15, swap=sw), 12.0, 0.0, 3.6)
        build_dugout(m, frame(8.5, BOARD_MIN + 0.1, swap=sw))
    return m


def build_hole():
    """수비 구멍 1개. 원점 중심, 바닥(y=0) 위에 얹는 얇은 링. x/z 로 이동시켜 사용."""
    m = Mesh(np.eye(3))
    r = 0.85
    cap(m, 'hole_dark', circle(0, 0, r), 0.03)
    c = (0, 0.0, 0)
    loop_strip(m, 'hole_dark', at(circle(0, 0, r), 0.03), at(circle(0, 0, r + 0.1), 0.13), 'in', (0, 1.0, 0))
    loop_strip(m, 'hole_rim', at(circle(0, 0, r + 0.1), 0.13), at(circle(0, 0, r + 0.28), 0.13), 'up')
    loop_strip(m, 'hole_rim', at(circle(0, 0, r + 0.28), 0.13), at(circle(0, 0, r + 0.42), 0.02), 'out', (0, -1.0, 0))
    return m


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, 'Assets', 'Models')
    os.makedirs(out, exist_ok=True)
    for name, mesh, title in (('stadium', build_stadium(), 'Toy baseball stadium'),
                              ('hole', build_hole(), 'Fielder hole (moving out pocket)')):
        mesh.write(os.path.join(out, name + '.obj'), name + '.mtl', title)
        allp = np.array([p for ts in mesh.tris.values() for t in ts for p in t[0]])
        print('%s.obj: %d triangles, bbox min %s max %s' % (
            name, mesh.count(), np.round(allp.min(0), 2), np.round(allp.max(0), 2)))
        for mat, ts in sorted(mesh.tris.items(), key=lambda kv: -len(kv[1])):
            print('   %-12s %5d' % (mat, len(ts)))


if __name__ == '__main__':
    main()
