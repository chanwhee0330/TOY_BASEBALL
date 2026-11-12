# -*- coding: utf-8 -*-
"""
토이 베이스볼 소품 OBJ 생성 스크립트 (공, 배트, 선수 인형)

실행:  python Tools/make_props.py
출력:  Assets/Models/ball.obj         공. 원점 = 중심, 반지름 BALL_R
       Assets/Models/bat.obj          배트. 원점 = 손잡이 끝(회전축), +X 방향으로 뻗음
       Assets/Models/player_red.obj   선수 인형(빨강 팀). 원점 = 발밑 중심, +Z 를 바라봄
       Assets/Models/player_blue.obj  선수 인형(파랑 팀)

선수 인형 하나를 투수, 타자, 주자에 모두 쓴다. 위치와 방향은 코드에서 정한다.
"""
import math
import os
import numpy as np

from PIL import Image

import make_stadium as ms
from make_stadium import Mesh

BALL_R = 0.30
BAT_LEN = 2.40
BALL_TEX = 'ball.png'   # 야구공 텍스처 (등장방형, 가로 2 : 세로 1)
BALL_TEX_W = 1024
STITCHES = 60           # 실밥 쌍의 수
SEGS = 12

ms.MATERIALS.update({
    'ball':      (0.98, 0.98, 0.96),
    'bat_wood':  (0.80, 0.60, 0.36),
    'bat_grip':  (0.20, 0.20, 0.22),
    'team_red':  (0.86, 0.25, 0.22),
    'team_blue': (0.20, 0.42, 0.80),
    'skin':      (0.96, 0.80, 0.66),
    'pants':     (0.95, 0.95, 0.93),
    'fig_base':  (0.17, 0.19, 0.22),
})
ms.TEXTURES['ball'] = BALL_TEX


def lathe(m, profile, segs=SEGS, cx=0.0, cz=0.0):
    """Y축 회전체. profile: [(반지름, 높이, 재질), ...] 를 아래에서 위로.
    각 구간은 시작점의 재질을 쓴다. 바깥쪽이 앞면."""
    for (r0, y0, mat), (r1, y1, _) in zip(profile, profile[1:]):
        for i in range(segs):
            t0, t1 = 2 * math.pi * i / segs, 2 * math.pi * (i + 1) / segs
            tm = (t0 + t1) / 2
            q = [(cx + r0 * math.cos(t0), y0, cz + r0 * math.sin(t0)),
                 (cx + r0 * math.cos(t1), y0, cz + r0 * math.sin(t1)),
                 (cx + r1 * math.cos(t1), y1, cz + r1 * math.sin(t1)),
                 (cx + r1 * math.cos(t0), y1, cz + r1 * math.sin(t0))]
            dy, dr = y1 - y0, r1 - r0
            m.face(mat, q, normal=(dy * math.cos(tm), -dr, dy * math.sin(tm)))


def sphere(m, mat, c, r, segs=16, rings=10, y_min=-1.0, uv=False):
    """부드러운 법선의 구. y_min > -1 이면 아래를 잘라 반구(모자 등)로 쓴다.
    uv=True 면 등장방형 텍스처 좌표(u = 경도, v = 위가 1)를 붙인다."""
    c = np.asarray(c, float)

    def pt(i, j):
        phi = math.pi * j / rings                 # 0 = 꼭대기
        th = 2 * math.pi * i / segs
        return np.array([math.sin(phi) * math.cos(th), math.cos(phi), math.sin(phi) * math.sin(th)])

    for j in range(rings):
        if math.cos(math.pi * (j + 1) / rings) < y_min - 1e-9:
            break
        for i in range(segs):
            d = [pt(i, j), pt(i + 1, j), pt(i + 1, j + 1), pt(i, j + 1)]
            t = [(i / segs, 1 - j / rings), ((i + 1) / segs, 1 - j / rings),
                 ((i + 1) / segs, 1 - (j + 1) / rings), (i / segs, 1 - (j + 1) / rings)] if uv else None
            m.face(mat, [c + r * q for q in d], away=c, smooth=d, uv=t)


def build_ball():
    m = Mesh(np.eye(3))
    sphere(m, 'ball', (0, 0, 0), BALL_R, segs=20, rings=12, uv=True)
    return m


def make_ball_texture(path, W=BALL_TEX_W):
    """야구공 실밥 곡선을 구 위에서 계산해 등장방형 텍스처로 그린다.
    sphere() 의 UV 와 같은 규칙: u = 경도 / 2pi, v = 위(+Y)가 1."""
    H = W // 2
    # 실밥 곡선 (단위 구 위의 닫힌 곡선, +Y 가 축)
    K, a = 2400, 0.4
    t = np.linspace(0, 4 * np.pi, K, endpoint=False)
    lat = np.pi / 2 - (np.pi / 2 - a) * np.cos(t)
    lon = t / 2 + a * np.sin(2 * t)
    P = np.stack([np.sin(lat) * np.cos(lon), np.cos(lat), np.sin(lat) * np.sin(lon)], 1)
    T = np.roll(P, -1, 0) - np.roll(P, 1, 0)
    T /= np.linalg.norm(T, axis=1, keepdims=True)
    side = np.cross(P, T)
    seg = np.linalg.norm(np.roll(P, -1, 0) - P, axis=1)
    S = np.concatenate([[0], np.cumsum(seg)[:-1]])
    period = seg.sum() / STITCHES

    img = np.zeros((H, W, 3), np.float32)
    base = np.array([0.97, 0.96, 0.92], np.float32)
    red = np.array([0.78, 0.12, 0.12], np.float32)
    line = np.array([0.80, 0.70, 0.66], np.float32)
    aa = 1.5 * np.pi / H                         # 가장자리 부드럽게 (약 1.5 픽셀)
    th = 2 * np.pi * (np.arange(W) + 0.5) / W
    Pf = P.astype(np.float32)
    for y in range(H):
        ph = np.pi * (y + 0.5) / H               # 위에서부터
        D = np.stack([np.sin(ph) * np.cos(th), np.full(W, np.cos(ph)), np.sin(ph) * np.sin(th)], 1)
        i = np.argmax(D.astype(np.float32) @ Pf.T, axis=1)
        off = D - P[i]
        d = np.abs(np.einsum('ij,ij->i', off, side[i]))
        along = S[i] + np.einsum('ij,ij->i', off, T[i])
        # V자 실밥: 곡선 양쪽으로 비스듬히
        f = ((along - 0.9 * d) / period) % 1.0
        da = np.abs(f - 0.5) * period
        st = (np.clip((0.016 - da) / aa + 0.5, 0, 1) * np.clip((d - 0.012) / aa, 0, 1)
              * np.clip((0.105 - d) / aa, 0, 1))
        ln = np.clip((0.006 - d) / aa + 0.5, 0, 1)
        row = base + (line - base) * ln[:, None]
        row = row + (red - row) * st[:, None]
        img[y] = row
    Image.fromarray((img * 255 + 0.5).astype(np.uint8)).save(path, optimize=True)
    print('%s: %dx%d, %d bytes' % (os.path.basename(path), W, H, os.path.getsize(path)))


def build_bat():
    # 회전체는 Y축 기준이라, x <-> y 를 바꿔 +X 방향으로 눕힌다
    m = Mesh(np.array([[0, 1, 0], [1, 0, 0], [0, 0, 1.0]]))
    L = BAT_LEN
    lathe(m, [(0.00, 0.00, 'bat_grip'), (0.11, 0.00, 'bat_grip'), (0.11, 0.07, 'bat_grip'),
              (0.06, 0.12, 'bat_grip'), (0.065, 0.85, 'bat_wood'), (0.13, L * 0.70, 'bat_wood'),
              (0.14, L - 0.16, 'bat_wood'), (0.10, L - 0.04, 'bat_wood'), (0.00, L, 'bat_wood')], segs=10)
    return m


def build_player(team):
    m = Mesh(np.eye(3))
    # 받침, 다리, 몸통
    lathe(m, [(0.55, 0.00, 'fig_base'), (0.55, 0.08, 'fig_base'), (0.48, 0.14, 'fig_base'),
              (0.30, 0.14, 'pants'), (0.27, 0.72, team), (0.40, 0.76, team),
              (0.42, 1.25, team), (0.33, 1.42, team), (0.14, 1.46, 'skin'),
              (0.14, 1.56, 'skin')])
    # 머리, 모자
    head = (0, 1.82, 0)
    sphere(m, 'skin', head, 0.32, segs=14, rings=8)
    sphere(m, team, (0, 1.86, 0), 0.345, segs=14, rings=8, y_min=0.0)
    brim = [(0.33 * math.cos(math.radians(a)), 0.33 * math.sin(math.radians(a))) for a in range(20, 161, 20)]
    tip = [(1.1 * x, z + 0.28) for x, z in brim]
    for i in range(len(brim) - 1):
        q = [(brim[i][0], 1.87, brim[i][1]), (brim[i + 1][0], 1.87, brim[i + 1][1]),
             (tip[i + 1][0], 1.84, tip[i + 1][1]), (tip[i][0], 1.84, tip[i][1])]
        m.face(team, q, normal=(0, 1, 0))
        m.face(team, q, normal=(0, -1, 0))
    # 팔 (살짝 벌린 상자) + 손
    for s in (-1, 1):
        A = ms.frame(0.47 * s, 0.0, y=1.36, pitch=0)
        tilt = 0.16 * s
        pts = []
        for h, w in ((-0.62, 0.085), (0.0, 0.10)):
            off = tilt * (-h)
            for u, v in ((-w, -w), (w, -w), (w, w), (-w, w)):
                pts.append(ms.tp(A, u + off, h, v))
        ms.hexa(m, pts, team)
        sphere(m, 'skin', ms.tp(A, tilt * 0.70, -0.70, 0), 0.115, segs=8, rings=6)
    return m


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, 'Assets', 'Models')
    os.makedirs(out, exist_ok=True)
    jobs = (('ball', build_ball(), 'Ball (origin = center)'),
            ('bat', build_bat(), 'Bat (origin = knob end / pivot, extends along +X)'),
            ('player_red', build_player('team_red'), 'Player figure, red team (origin = feet, faces +Z)'),
            ('player_blue', build_player('team_blue'), 'Player figure, blue team (origin = feet, faces +Z)'))
    make_ball_texture(os.path.join(out, BALL_TEX))
    for name, mesh, title in jobs:
        mesh.write(os.path.join(out, name + '.obj'), name + '.mtl', title)
        allp = np.array([p for ts in mesh.tris.values() for t in ts for p in t[0]])
        print('%s.obj: %d triangles, bbox min %s max %s' % (
            name, mesh.count(), np.round(allp.min(0), 2), np.round(allp.max(0), 2)))


if __name__ == '__main__':
    main()
