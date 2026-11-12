# -*- coding: utf-8 -*-
"""
OBJ 미리보기 렌더러 (numpy + Pillow, z-버퍼 소프트웨어 래스터라이저)
뒷면 제거를 켜고 그리므로 면 방향(와인딩)이 틀리면 구멍으로 보인다.

사용: python Tools/preview_obj.py <obj> <out.png> [ex ey ez  tx ty tz  fov]
"""
import math
import os
import sys
import numpy as np
from PIL import Image


def load(path):
    V, tris, cols, mats, cur = [], [], [], {}, None
    folder = os.path.dirname(path)
    for line in open(path):
        s = line.split()
        if not s:
            continue
        if s[0] == 'mtllib':
            name = None
            for l in open(os.path.join(folder, s[1])):
                t = l.split()
                if t and t[0] == 'newmtl':
                    name = t[1]
                    mats[name] = [(.8, .8, .8), (0, 0, 0)]
                elif t and t[0] == 'Kd':
                    mats[name][0] = tuple(map(float, t[1:4]))
                elif t and t[0] == 'Ke':
                    mats[name][1] = tuple(map(float, t[1:4]))
        elif s[0] == 'v':
            V.append(tuple(map(float, s[1:4])))
        elif s[0] == 'usemtl':
            cur = s[1]
        elif s[0] == 'f':
            idx = [int(p.split('/')[0]) - 1 for p in s[1:]]
            for i in range(1, len(idx) - 1):
                tris.append((idx[0], idx[i], idx[i + 1]))
                cols.append(mats[cur])
    return np.array(V), np.array(tris), cols


def render(path, out, eye, target, fov=45.0, W=1200, H=800, ss=2, cull=True):
    V, T, cols = load(path)
    eye, target = np.array(eye, float), np.array(target, float)
    f = target - eye
    f /= np.linalg.norm(f)
    r = np.cross(f, [0, 1, 0])
    r /= np.linalg.norm(r)
    u = np.cross(r, f)
    w, h = W * ss, H * ss
    cam = (V - eye) @ np.stack([r, u, -f]).T          # 카메라 공간 (보는 방향 = -z)
    foc = (h / 2) / math.tan(math.radians(fov) / 2)
    z = -cam[:, 2]
    sx = w / 2 + cam[:, 0] / np.maximum(z, 1e-6) * foc
    sy = h / 2 - cam[:, 1] / np.maximum(z, 1e-6) * foc

    img = np.zeros((h, w, 3)) + np.array([0.86, 0.94, 0.90])
    zbuf = np.full((h, w), np.inf)
    light = np.array([0.35, 0.85, 0.40])
    light /= np.linalg.norm(light)
    culled = 0
    for (a, b, c), (kd, ke) in zip(T, cols):
        if z[a] < 0.1 or z[b] < 0.1 or z[c] < 0.1:
            continue
        n = np.cross(V[b] - V[a], V[c] - V[a])
        ln = np.linalg.norm(n)
        if ln < 1e-12:
            continue
        n /= ln
        if np.dot(n, eye - V[a]) <= 0:
            if cull:
                culled += 1
                continue
            n = -n
        x0, y0, x1, y1, x2, y2 = sx[a], sy[a], sx[b], sy[b], sx[c], sy[c]
        minx, maxx = int(max(0, math.floor(min(x0, x1, x2)))), int(min(w - 1, math.ceil(max(x0, x1, x2))))
        miny, maxy = int(max(0, math.floor(min(y0, y1, y2)))), int(min(h - 1, math.ceil(max(y0, y1, y2))))
        if minx > maxx or miny > maxy:
            continue
        den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(den) < 1e-9:
            continue
        xs, ys = np.meshgrid(np.arange(minx, maxx + 1) + 0.5, np.arange(miny, maxy + 1) + 0.5)
        l0 = ((y1 - y2) * (xs - x2) + (x2 - x1) * (ys - y2)) / den
        l1 = ((y2 - y0) * (xs - x2) + (x0 - x2) * (ys - y2)) / den
        l2 = 1 - l0 - l1
        inside = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
        if not inside.any():
            continue
        zz = 1.0 / (l0 / z[a] + l1 / z[b] + l2 / z[c])
        sub = zbuf[miny:maxy + 1, minx:maxx + 1]
        m = inside & (zz < sub)
        sub[m] = zz[m]
        shade = 0.42 + 0.58 * max(0.0, float(np.dot(n, light)))
        col = np.minimum(1.0, np.array(kd) * shade + np.array(ke) * 0.5)
        img[miny:maxy + 1, minx:maxx + 1][m] = col
    img = img.reshape(H, ss, W, ss, 3).mean(axis=(1, 3))
    Image.fromarray((img * 255).astype(np.uint8)).save(out)
    print('saved %s  (%d tris, %d back-facing culled)' % (out, len(T), culled))


if __name__ == '__main__':
    a = sys.argv
    nums = list(map(float, a[3:]))
    eye = nums[0:3] if len(nums) >= 3 else (0, 26, 22)
    tgt = nums[3:6] if len(nums) >= 6 else (0, 0, -11)
    fov = nums[6] if len(nums) >= 7 else 45.0
    render(a[1], a[2], eye, tgt, fov)
