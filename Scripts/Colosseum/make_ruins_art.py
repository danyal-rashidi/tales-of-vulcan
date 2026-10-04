"""Generate ruin art: Roman Doric column meshes (OBJ) + limestone and rocky-sand textures.

Outputs to SourceArt/Colosseum/:
  SM_RomanColumn.obj, SM_RomanColumn_BrokenA.obj, SM_RomanColumn_BrokenB.obj  (Z-up, cm)
  T_Limestone_D/N/R.png   weathered pale stone for the columns
  T_RockySand_D/N/R.png   sand with half-buried rocks and pebbles for the ground
"""
import math
import os

import numpy as np
from PIL import Image

from make_textures import OUT, blur, normal_from_height, save, tile_noise

rng = np.random.default_rng(753)

# ------------------------------------------------------------------ column mesh
SEG = 80            # around the column (4 per flute)
FLUTES = 20
H = 520.0           # total height, cm


class Mesh:
    def __init__(self):
        self.v, self.vt, self.vn, self.f = [], [], [], []

    def add_grid(self, pts, uvs):
        """pts/uvs: [rings][SEG+1] grid, smooth normals, quads -> tris."""
        pts, uvs = np.asarray(pts, float), np.asarray(uvs, float)
        R, C = pts.shape[:2]
        # smooth normals from neighbouring differences
        du = np.roll(pts, -1, 1) - np.roll(pts, 1, 1)
        du[:, 0] = pts[:, 1] - pts[:, -2]
        du[:, -1] = du[:, 0]
        dv = np.zeros_like(pts)
        dv[1:-1] = pts[2:] - pts[:-2]
        dv[0] = pts[1] - pts[0]
        dv[-1] = pts[-1] - pts[-2]
        n = np.cross(du, dv)
        n /= np.linalg.norm(n, axis=-1, keepdims=True) + 1e-9
        base = len(self.v)
        for r in range(R):
            for c in range(C):
                self.v.append(pts[r, c]); self.vt.append(uvs[r, c]); self.vn.append(n[r, c])
        for r in range(R - 1):
            for c in range(C - 1):
                a = base + r * C + c
                b, d, e = a + 1, a + C, a + C + 1
                self.f += [(a, b, d), (b, e, d)]  # counter-clockwise seen from outside

    def add_quad(self, p, n, uv):
        base = len(self.v)
        for i in range(4):
            self.v.append(p[i]); self.vn.append(n); self.vt.append(uv[i])
        self.f += [(base, base + 1, base + 2), (base, base + 2, base + 3)]

    def add_box(self, cx, cy, z0, w, d, h):
        x0, x1, y0, y1, z1 = cx - w / 2, cx + w / 2, cy - d / 2, cy + d / 2, z0 + h
        s = 1 / 200.0
        faces = [  # (corners CCW seen from outside, normal)
            ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)),
            ([(x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0)], (0, 0, -1)),
            ([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0)),
            ([(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)], (0, 1, 0)),
            ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], (1, 0, 0)),
            ([(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)], (-1, 0, 0)),
        ]
        for corners, n in faces:
            a = np.array(corners, float)
            ax = [i for i in range(3) if n[i] == 0]
            uv = [(c[ax[0]] * s, c[ax[1]] * s) for c in a]
            self.add_quad(a, n, uv)

    def add_cap(self, ring, z_fn=None):
        """Fan-fill a closed ring (SEG+1 pts) facing up."""
        ring = np.asarray(ring, float)
        center = ring[:-1].mean(0)
        base = len(self.v)
        self.v.append(center); self.vn.append((0, 0, 1)); self.vt.append((center[0] / 200, center[1] / 200))
        for p in ring:
            self.v.append(p); self.vn.append((0, 0, 1)); self.vt.append((p[0] / 200, p[1] / 200))
        for i in range(len(ring) - 1):
            self.f.append((base, base + 1 + i, base + 2 + i))

    def write(self, path):
        with open(path, "w") as fh:
            fh.write("# Roman Doric column, Z-up, centimetres\n")
            for p in self.v:
                fh.write("v %.4f %.4f %.4f\n" % tuple(p))
            for t in self.vt:
                fh.write("vt %.5f %.5f\n" % (t[0], t[1]))
            for n in self.vn:
                fh.write("vn %.5f %.5f %.5f\n" % tuple(n))
            for a, b, c in self.f:  # OBJ is 1-based; every face is CCW seen from outside
                fh.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a + 1, a + 1, a + 1, b + 1, b + 1, b + 1, c + 1, c + 1, c + 1))
        print("wrote", os.path.basename(path), len(self.v), "verts")


def revolve(mesh, profile, fluted=False, top_jag=None):
    """profile: list of (z, radius). Fluting applied where fluted(z) is True."""
    th = np.linspace(0, 2 * np.pi, SEG + 1)
    pts, uvs = [], []
    for i, (z, r) in enumerate(profile):
        ring, uvr = [], []
        for t in th:
            rr = r
            if fluted and fluted(z):
                rr = r - 3.2 * (0.5 + 0.5 * math.cos(FLUTES * t)) ** 0.7  # concave channels
            zz = z
            if top_jag is not None and i == len(profile) - 1:
                zz = z + top_jag(t)
            ring.append((rr * math.cos(t), rr * math.sin(t), zz))
            uvr.append((t / (2 * np.pi) * (2 * np.pi * r) / 200.0, zz / 200.0))
        pts.append(ring); uvs.append(uvr)
    mesh.add_grid(pts, uvs)
    return pts[-1]


def column(break_at=None, seed=0):
    m = Mesh()
    jr = np.random.default_rng(seed)
    m.add_box(0, 0, 0, 120, 120, 26)                       # plinth
    base = [(26 + 20 * k / 8, 44 + 9 * math.sin(math.pi * k / 8)) for k in range(9)]  # torus moulding
    revolve(m, base)

    shaft_bot, shaft_top = 46.0, H - 62.0
    top = shaft_top if break_at is None else break_at
    n = max(4, int((top - shaft_bot) / 12))
    shaft = []
    for k in range(n + 1):
        z = shaft_bot + (top - shaft_bot) * k / n
        u = (z - shaft_bot) / (shaft_top - shaft_bot)
        shaft.append((z, 38.0 - 6.0 * u ** 1.6 + 1.2 * math.sin(math.pi * u)))  # entasis
    fl = lambda z: True
    if break_at is None:
        revolve(m, shaft, fluted=fl)
        ech = [(shaft_top + 22 * k / 8, 33 + 22 * (k / 8) ** 0.6) for k in range(9)]  # echinus
        ring = revolve(m, ech)
        m.add_box(0, 0, shaft_top + 22, 122, 122, 40)        # abacus
    else:
        phase = jr.random(4) * 6.28
        amp = jr.uniform(10, 26)
        jag = lambda t: amp * (0.45 * math.sin(t + phase[0]) + 0.3 * math.sin(3 * t + phase[1])
                               + 0.2 * math.sin(7 * t + phase[2]) + 0.12 * math.sin(13 * t + phase[3]))
        ring = revolve(m, shaft, fluted=fl, top_jag=jag)
        m.add_cap(ring)
    return m


# ------------------------------------------------------------------ textures
def limestone():
    S = 2048
    large = tile_noise(S, 3, 5)
    mid = tile_noise(S, 12, 4)
    grain = tile_noise(S, 160, 2)
    pits = np.clip((tile_noise(S, 90, 3) - 0.63) * 6, 0, 1)
    veins = np.clip(1 - np.abs(tile_noise(S, 5, 5) - 0.5) * 45, 0, 1) * (tile_noise(S, 3, 2) > 0.5)
    stains = np.clip((tile_noise(S, 2, 5, 0.6) - 0.5) * 2.2, 0, 1)
    y = np.arange(S)[:, None].repeat(S, 1) / S
    drips = blur(np.clip(tile_noise(S, 40, 2) * (0.5 + 0.5 * np.sin(y * 6.28 * 3)) - 0.55, 0, 1), 2) * 3

    pale = np.array([0.88, 0.82, 0.70])
    warm = np.array([0.78, 0.66, 0.50])
    dirt = np.array([0.42, 0.36, 0.28])
    t = np.clip(0.55 * large + 0.3 * mid + 0.15 * grain, 0, 1)[..., None]
    col = pale + (warm - pale) * t
    g = np.clip(stains * 0.6 + drips * 0.3 + pits * 0.5 + veins * 0.7, 0, 1)[..., None] * 0.6
    col = col * (1 - g) + dirt * g
    col *= (0.94 + 0.12 * grain)[..., None]
    height = 0.6 + 0.2 * mid + 0.08 * grain - 0.25 * pits - 0.3 * veins
    save("T_Limestone_D", col)
    save("T_Limestone_N", normal_from_height(blur(height, 1), 5.0))
    save("T_Limestone_R", np.clip(0.72 + 0.12 * grain + 0.1 * pits - 0.06 * large, 0, 1))


def rocky_sand():
    S = 2048
    y = np.arange(S)[:, None].repeat(S, 1).astype(np.float32)
    warp = tile_noise(S, 4, 4)
    ripples = (0.5 + 0.5 * np.sin((y / S) * np.pi * 2 * 26 + warp * 14)) ** 1.6
    grain = tile_noise(S, 256, 2)
    patches = tile_noise(S, 3, 4)
    sand_h = 0.25 * ripples * (0.4 + 0.6 * patches) + 0.12 * grain

    light = np.array([0.82, 0.70, 0.50])
    dark = np.array([0.62, 0.49, 0.34])
    st = np.clip(0.6 * patches + 0.25 * (1 - ripples) + 0.15 * grain, 0, 1)[..., None]
    col = light + (dark - light) * st

    rock_h = np.zeros((S, S), np.float32)
    rock_col = np.zeros((S, S, 3), np.float32)
    rough = np.full((S, S), 0.9, np.float32)
    rnoise = tile_noise(S, 48, 3)
    palette = np.array([[0.55, 0.48, 0.40], [0.66, 0.58, 0.47], [0.47, 0.42, 0.37],
                        [0.72, 0.62, 0.48], [0.58, 0.45, 0.34]])
    # big half-buried rocks, medium stones, many pebbles
    for count, rmin, rmax in ((26, 70, 140), (140, 22, 55), (1600, 5, 14)):
        for _ in range(count):
            cx, cy = rng.uniform(0, S, 2)
            r = rng.uniform(rmin, rmax)
            ang = rng.uniform(0, np.pi)
            el = rng.uniform(0.55, 1.0)
            w = int(r * 1.4) + 2
            xs = (np.arange(int(cx) - w, int(cx) + w)) % S
            ys = (np.arange(int(cy) - w, int(cy) + w)) % S
            gx, gy = np.meshgrid(np.arange(-w, w) + (int(cx) - cx), np.arange(-w, w) + (int(cy) - cy))
            rx = gx * math.cos(ang) + gy * math.sin(ang)
            ry = (-gx * math.sin(ang) + gy * math.cos(ang)) / el
            d = np.sqrt(rx ** 2 + ry ** 2) / r
            d = d + (rnoise[np.ix_(ys, xs)] - 0.5) * 0.5   # irregular outline
            hgt = np.sqrt(np.clip(1 - d ** 2, 0, 1)) * (r / rmax) ** 0.5 * 0.9
            facet = np.round(hgt * 5) / 5 * 0.4 + hgt * 0.6   # chunky faceted look
            sub = np.ix_(ys, xs)
            mask = facet > rock_h[sub]
            rock_h[sub] = np.where(mask, facet, rock_h[sub])
            c = palette[rng.integers(len(palette))] * rng.uniform(0.85, 1.1)
            shade = (0.75 + 0.35 * hgt)[..., None]
            rc = rock_col[sub]
            rock_col[sub] = np.where(mask[..., None], c * shade, rc)
            rough[sub] = np.where(mask, 0.7 + 0.1 * rng.random(), rough[sub])

    bury = 0.08 + 0.1 * tile_noise(S, 8, 3)                  # sand drifts over rock bases
    is_rock = rock_h > bury
    rock_vis = np.clip((rock_h - bury) * 12, 0, 1)[..., None]
    ao = np.clip(1 - blur((rock_h > 0.02).astype(np.float32), 6) * 0.45, 0.55, 1)[..., None]
    col = col * ao
    rock_col = rock_col * (0.9 + 0.2 * tile_noise(S, 128, 2))[..., None]
    col = col * (1 - rock_vis) + rock_col * rock_vis
    height = np.where(is_rock, rock_h, 0) + sand_h * 0.5
    save("T_RockySand_D", col)
    save("T_RockySand_N", normal_from_height(blur(height, 1), 9.0))
    save("T_RockySand_R", np.where(is_rock, rough, 0.92))


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    column().write(os.path.join(OUT, "SM_RomanColumn.obj"))
    column(break_at=300.0, seed=1).write(os.path.join(OUT, "SM_RomanColumn_BrokenA.obj"))
    column(break_at=170.0, seed=2).write(os.path.join(OUT, "SM_RomanColumn_BrokenB.obj"))
    limestone()
    rocky_sand()
