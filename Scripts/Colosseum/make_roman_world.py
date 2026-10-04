"""Generate the world around the colosseum as OBJ meshes (Z-up, cm) in SourceArt/Colosseum/:

  SM_Dunes.obj            desert terrain: flat around the colosseum, dunes rising to the horizon
  SM_Aqueduct.obj         two-tier aqueduct, collapsed bays and a broken end, rubble at its feet
  SM_TriumphalArch.obj    three-bay triumphal arch with attic, weathered
  SM_ArcadeRuin.obj       tall ruined arcade (the painting reference), top crumbling away
  SM_TemplePodium.obj     temple platform with front steps (columns are placed in Unreal)
  SM_Obelisk.obj          tapered obelisk on a pedestal
  SM_RomanBlock_A/B/C.obj loose chipped 1 m blocks for rubble and entablatures

Everything masonry is built from individual stone blocks: slightly rotated, shrunk and
corner-jittered (chipped), with blocks missing near ruined edges.
"""
import math
import os

import numpy as np

from make_textures import OUT

UV = 1 / 200.0


class Mesh:
    def __init__(self):
        self.v, self.vt, self.vn, self.f = [], [], [], []

    # ---- a closed hexahedron from 8 corners (bit 0 = +x, bit 1 = +y, bit 2 = +z)
    def add_hex(self, c):
        c = np.asarray(c, float)
        center = c.mean(0)
        faces = [(0, 2, 6, 4), (1, 5, 7, 3), (0, 4, 5, 1), (2, 3, 7, 6), (0, 1, 3, 2), (4, 6, 7, 5)]
        for q in faces:
            p = c[list(q)]
            n = np.cross(p[1] - p[0], p[2] - p[0])
            if np.dot(n, p.mean(0) - center) < 0:
                p = p[::-1]
                n = -n
            n = n / (np.linalg.norm(n) + 1e-9)
            ax = np.argsort(np.abs(n))[:2]  # project UVs onto the two flattest axes
            base = len(self.v)
            for k in range(4):
                self.v.append(p[k]); self.vn.append(n); self.vt.append((p[k][ax[0]] * UV, p[k][ax[1]] * UV))
            self.f += [(base, base + 1, base + 2), (base, base + 2, base + 3)]

    def add_grid(self, pts):
        """pts[rows][cols] surface; rows x cols must be ordered so cross(d/dcol, d/drow) faces out."""
        pts = np.asarray(pts, float)
        R, C = pts.shape[:2]
        du = np.gradient(pts, axis=1)
        dv = np.gradient(pts, axis=0)
        n = np.cross(du, dv)
        n /= np.linalg.norm(n, axis=-1, keepdims=True) + 1e-9
        base = len(self.v)
        for r in range(R):
            for c in range(C):
                p = pts[r, c]
                self.v.append(p); self.vn.append(n[r, c]); self.vt.append((p[0] * UV, p[1] * UV))
        for r in range(R - 1):
            for c in range(C - 1):
                a = base + r * C + c
                b, d, e = a + 1, a + C, a + C + 1
                self.f += [(a, b, d), (b, e, d)]

    def write(self, name):
        path = os.path.join(OUT, name + ".obj")
        with open(path, "w") as fh:
            fh.write("# %s, Z-up, centimetres\n" % name)
            for p in self.v:
                fh.write("v %.2f %.2f %.2f\n" % tuple(p))
            for t in self.vt:
                fh.write("vt %.4f %.4f\n" % tuple(t))
            for n in self.vn:
                fh.write("vn %.4f %.4f %.4f\n" % tuple(n))
            for a, b, c in self.f:
                fh.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a + 1, a + 1, a + 1, b + 1, b + 1, b + 1, c + 1, c + 1, c + 1))
        print("wrote %-22s %7d verts %7d tris" % (name, len(self.v), len(self.f)))


def rot_matrix(yaw, pitch, roll):
    cy, sy, cp, sp, cr, sr = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch), math.cos(roll), math.sin(roll)
    rz = np.array([[cy, -sy, 0], [sy, cy, 0], [0, 0, 1]])
    ry = np.array([[cp, 0, sp], [0, 1, 0], [-sp, 0, cp]])
    rx = np.array([[1, 0, 0], [0, cr, -sr], [0, sr, cr]])
    return rz @ ry @ rx


def stone(m, rng, center, size, yaw=0.0, wear=1.0, tilt=0.0):
    """One weathered block: shrunk a little, corners knocked about, slightly crooked."""
    size = np.asarray(size, float) * (1 - rng.uniform(0.0, 0.035 * wear, 3))
    R = rot_matrix(yaw + rng.normal(0, 0.006 * wear), rng.normal(0, 0.005 * wear) + tilt, rng.normal(0, 0.005 * wear))
    corners = []
    for i in range(8):
        local = np.array([(1 if i & 1 else -1), (1 if i & 2 else -1), (1 if i & 4 else -1)]) * size / 2
        local += rng.uniform(-1, 1, 3) * size * 0.03 * wear  # chipped corners
        corners.append(R @ local + center)
    m.add_hex(corners)


# ------------------------------------------------------------------ masonry wall with arches
def arched_wall(m, rng, length, depth, top, openings, base=0.0, row_h=60.0, ruin=None, y0=0.0,
                cornice_at=(), wear=1.0, missing=0.03):
    """Fill a wall [0,length] x [y0-depth/2, y0+depth/2] x [base, top] with coursed blocks,
    leaving arched openings. openings: list of (center_x, half_span, springing_z).
    ruin(x) -> max height at x (crumbled profile)."""
    ring = 75.0  # voussoir ring thickness
    rows = int((top - base) / row_h)
    for r in range(rows):
        z0 = base + r * row_h
        zc = z0 + row_h / 2
        # solid intervals along x for this course
        cuts = []
        for cx, hs, spring in openings:
            if zc < spring:
                cuts.append((cx - hs, cx + hs))
            elif zc < spring + hs + ring:
                w = math.sqrt(max((hs + ring) ** 2 - (zc - spring) ** 2, 0))
                cuts.append((cx - w, cx + w))
        cuts.sort()
        segs, x = [], 0.0
        for a, b in cuts:
            if a > x:
                segs.append((x, a))
            x = max(x, b)
        if x < length:
            segs.append((x, length))
        stagger = (r % 2) * 0.5
        for a, b in segs:
            x = a
            first = True
            while x < b - 5:
                bl = rng.uniform(80, 140) * (0.5 + stagger if first and stagger else 1)
                bl = min(bl, b - x)
                first = False
                xm = x + bl / 2
                limit = ruin(xm) if ruin else top
                if zc < limit and rng.random() > missing * (1 + 3 * max(0, (zc - base) / max(top - base, 1) - 0.7)):
                    stone(m, rng, (xm, y0, zc), (bl - 3, depth, row_h - 3), wear=wear)
                x += bl
        if any(abs(z0 - c) < row_h / 2 for c in cornice_at):  # projecting string course
            x = 0.0
            while x < length:
                bl = rng.uniform(100, 160)
                if (ruin(x) if ruin else top) > zc and rng.random() > 0.08:
                    stone(m, rng, (x + bl / 2, y0, zc), (bl - 3, depth + 50, row_h * 0.8), wear=wear)
                x += bl
    # voussoirs: wedge stones around each arch
    for cx, hs, spring in openings:
        n = max(7, int(math.pi * hs / 55) | 1)
        for k in range(n):
            ang = math.pi * (k + 0.5) / n
            rmid = hs + ring / 2
            px, pz = cx + rmid * math.cos(ang), spring + rmid * math.sin(ang)
            if (ruin(px) if ruin else top) <= pz or rng.random() < missing:
                continue
            tang = math.pi * hs / n
            R = rot_matrix(0, -(ang - math.pi / 2), 0)
            stone(m, rng, (px, y0, pz), (tang - 4, depth + 6, ring - 4), wear=wear)
            # rotate the last block about its centre so it points at the arch centre
            c = np.array([px, y0, pz])
            for i in range(len(m.v) - 24, len(m.v)):
                m.v[i] = R @ (np.asarray(m.v[i]) - c) + c
                m.vn[i] = R @ np.asarray(m.vn[i])


def rubble(m, rng, cx, cy, radius, count, zbase=0.0, size=(60, 140)):
    for _ in range(count):
        a = rng.uniform(0, 2 * math.pi)
        d = radius * math.sqrt(rng.random())
        s = rng.uniform(*size)
        dims = (s * rng.uniform(0.8, 1.6), s * rng.uniform(0.6, 1.1), s * rng.uniform(0.4, 0.8))
        stone(m, rng, (cx + d * math.cos(a), cy + d * math.sin(a), zbase + dims[2] * 0.35),
              dims, yaw=rng.uniform(0, math.pi), wear=2.0, tilt=rng.normal(0, 0.15))


# ------------------------------------------------------------------ buildings
def aqueduct():
    rng = np.random.default_rng(31)
    m = Mesh()
    bays, span, pier = 10, 600.0, 300.0
    L = bays * (span + pier) + pier
    t1_spring, t1_top = 1400.0, 2050.0
    t2_spring, t2_top = t1_top + 700.0, t1_top + 1250.0
    channel_top = t2_top + 180.0

    def ruin(x):
        h = channel_top
        if x > L * 0.83:  # broken end crumbling away
            h = channel_top - (x - L * 0.83) / (L * 0.17) * (channel_top + 200) + (90 * math.sin(x * 0.031) + 50 * math.sin(x * 0.077))
        if L * 0.52 < x < L * 0.64:  # collapsed bays: only stumps remain
            h = min(h, 350 + 250 * math.sin(x * 0.01) ** 2)
        if L * 0.45 < x < L * 0.52 or L * 0.64 < x < L * 0.70:  # torn edges of the gap
            h = min(h, t1_spring + 300 + (120 * math.sin(x * 0.023) + 60 * math.sin(x * 0.091)))
        return h

    o1 = [(pier + span / 2 + k * (span + pier), span / 2, t1_spring) for k in range(bays)]
    arched_wall(m, rng, L, 300, t1_top, o1, ruin=ruin, cornice_at=(t1_top - 60,))
    o2 = [(pier + span / 2 + k * (span + pier), span / 2, t2_spring) for k in range(bays)]
    arched_wall(m, rng, L, 240, t2_top, o2, base=t1_top, ruin=ruin, cornice_at=(t2_top - 60,))
    # water channel (specus): two low side walls on top
    for side in (-1, 1):
        arched_wall(m, rng, L, 50, channel_top, [], base=t2_top, ruin=ruin, y0=side * 95, row_h=45)
    rubble(m, rng, L * 0.58, 0, 900, 70)
    rubble(m, rng, L * 0.95, 0, 700, 40)
    m.write("SM_Aqueduct")


def triumphal_arch():
    rng = np.random.default_rng(47)
    m = Mesh()
    L, D, top = 2500.0, 1000.0, 2100.0
    openings = [(L / 2, 450.0, 950.0), (L / 2 - 800, 190.0, 520.0), (L / 2 + 800, 190.0, 520.0)]

    def ruin(x):  # attic corner broken off
        return top - max(0, (x - L * 0.78)) * 1.6 + 25 * math.sin(x * 0.05)

    arched_wall(m, rng, L, D, top, openings, ruin=ruin, cornice_at=(1500.0, 1560.0), wear=1.2, missing=0.015)
    # plinth steps
    for k, (w, d) in enumerate([(L + 200, D + 200), (L + 100, D + 100)]):
        x = -100 + k * 50
        while x < L + 50 - k * 50:
            bl = rng.uniform(120, 200)
            stone(m, rng, (x + bl / 2, 0, -45 + k * 30), (bl - 3, d, 30), wear=0.8)
            x += bl
    rubble(m, rng, L * 0.9, 600, 500, 18)
    m.write("SM_TriumphalArch")


def arcade_ruin():
    rng = np.random.default_rng(59)
    m = Mesh()
    L, D, top = 2700.0, 450.0, 2600.0
    openings = [(450 + k * 900, 330.0, 1150.0) for k in range(3)]

    def ruin(x):  # crumbles diagonally down to the right, ragged
        return top - max(0, x - 600) * 0.75 + 140 * math.sin(x * 0.013) + 60 * math.sin(x * 0.047)

    arched_wall(m, rng, L, D, top, openings, ruin=ruin, cornice_at=(1700.0,), wear=1.4, missing=0.05)
    rubble(m, rng, L * 0.8, 500, 800, 45)
    m.write("SM_ArcadeRuin")


def temple_podium():
    rng = np.random.default_rng(71)
    m = Mesh()
    W, Dp, H = 2400.0, 1500.0, 300.0
    for r in range(int(H / 60)):
        zc = r * 60 + 30
        for (x0, x1, y) in [(0, W, -Dp / 2), (0, W, Dp / 2)]:
            x = x0
            while x < x1:
                bl = min(rng.uniform(90, 150), x1 - x)
                stone(m, rng, (x + bl / 2, y, zc), (bl - 3, 120, 57))
                x += bl
        for xe in (0, W):
            y = -Dp / 2
            while y < Dp / 2:
                bl = min(rng.uniform(90, 150), Dp / 2 - y)
                stone(m, rng, (xe, y + bl / 2, zc), (120, bl - 3, 57))
                y += bl
    # paved top
    for i in range(int(W / 150)):
        for j in range(int(Dp / 150)):
            if rng.random() > 0.06:
                stone(m, rng, (75 + i * 150, -Dp / 2 + 75 + j * 150, H - 15), (146, 146, 30), wear=0.8)
    # front steps (on -x side)
    for k in range(5):
        y = -Dp / 2 + 150
        while y < Dp / 2 - 150:
            bl = rng.uniform(120, 200)
            stone(m, rng, (-60 - (4 - k) * 50, y + bl / 2, 30 + k * 60), (100, bl - 3, 57), wear=0.9)
            y += bl
    m.write("SM_TemplePodium")


def obelisk():
    rng = np.random.default_rng(83)
    m = Mesh()
    stone(m, rng, (0, 0, 150), (450, 450, 300), wear=0.6)
    stone(m, rng, (0, 0, 340), (380, 380, 80), wear=0.6)
    b0, b1, h = 130.0, 90.0, 2300.0
    z = 380.0
    c = []
    for i in range(8):
        s = b1 if i & 4 else b0
        c.append((s * (1 if i & 1 else -1) + rng.normal(0, 3), s * (1 if i & 2 else -1) + rng.normal(0, 3), z + (h if i & 4 else 0)))
    m.add_hex(c)
    tip = (0, 0, z + h + 220)
    top = [c[4], c[5], c[7], c[6]]
    for k in range(4):  # pyramidion
        a, b = np.array(top[k]), np.array(top[(k + 1) % 4])
        n = np.cross(b - a, np.array(tip) - a); n /= np.linalg.norm(n)
        base = len(m.v)
        for p in (a, b, tip):
            m.v.append(np.asarray(p, float)); m.vn.append(n); m.vt.append((p[0] * UV + p[1] * UV, p[2] * UV))
        m.f.append((base, base + 1, base + 2))
    m.write("SM_Obelisk")


def loose_blocks():
    for i, (name, dims) in enumerate([("SM_RomanBlock_A", (100, 100, 100)), ("SM_RomanBlock_B", (100, 100, 100)),
                                       ("SM_RomanBlock_C", (100, 100, 100))]):
        rng = np.random.default_rng(100 + i)
        m = Mesh()
        stone(m, rng, (0, 0, 50), dims, wear=1.8)
        m.write(name)


# ------------------------------------------------------------------ dunes
def value_noise(x, y, scale, seed):
    gx, gy = x / scale, y / scale
    x0, y0 = np.floor(gx), np.floor(gy)
    fx, fy = gx - x0, gy - y0
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)

    def h(ix, iy):
        ix, iy = np.asarray(ix).astype(np.int64), np.asarray(iy).astype(np.int64)
        n = (ix * 374761393 + iy * 668265263 + seed * 1442695041) & 0xFFFFFFFF
        n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
        return (n & 0xFFFF) / 65535.0

    v00, v10, v01, v11 = h(x0, y0), h(x0 + 1, y0), h(x0, y0 + 1), h(x0 + 1, y0 + 1)
    return (v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy


def fbm(x, y, scale, seed, octaves=4):
    out, amp, tot = 0, 1.0, 0
    for o in range(octaves):
        out = out + amp * value_noise(x, y, scale / 2 ** o, seed + o)
        tot += amp
        amp *= 0.5
    return out / tot


CX, CY = -44.0, -5.0  # colosseum centre (world)
GROUND = -71.0        # arena / surrounding ground height


def dune_height(x, y):
    """Shared with the placement script: ground height at world x, y (cm)."""
    dx, dy = x - CX, y - CY
    re = np.sqrt((dx / 5800.0) ** 2 + (dy / 4800.0) ** 2)
    ramp = np.clip((re - 2.6) / (9.0 - 2.6), 0, 1) ** 1.5
    far = np.clip((re - 6.0) / 20.0, 0, 1)
    wind = math.radians(25)
    u = (x * math.cos(wind) + y * math.sin(wind)) / 9000.0 + 1.8 * fbm(x, y, 30000, 3)
    p = u - np.floor(u)
    profile = np.where(p < 0.75, (p / 0.75) ** 1.4, (1 - p) / 0.25)  # gentle windward, steep lee side
    dunes = 2400 * profile * (0.5 + 0.7 * fbm(x, y, 20000, 7))
    big = 9000 * fbm(x, y, 90000, 11, 3) * far + 25000 * np.clip((re - 40.0) / 120.0, 0, 1) * fbm(x, y, 400000, 13, 2)
    ripples = 25 * np.sin((x * math.cos(wind) + y * math.sin(wind)) / 140.0)
    h = GROUND + ramp * (dunes + big) + ripples * np.clip(re - 1.2, 0, 1)
    return np.where(re < 1.02, GROUND - 40.0, h)  # tucked under the colosseum floor, no z-fighting


def dunes():
    m = Mesh()
    seg, rings = 288, 200
    s = np.concatenate([[0.0], np.geomspace(0.03, 200.0, rings)])  # from under the colosseum out to ~11 km
    th = np.linspace(0, 2 * np.pi, seg + 1)
    rows = []
    for t in th:  # rows = angle, cols = radius -> cross(d/dr, d/dtheta) points up
        xs = CX + 5800 * s * math.cos(t)
        ys = CY + 4800 * s * math.sin(t)
        zs = dune_height(xs, ys)
        rows.append(np.stack([xs, ys, zs], -1))
    m.add_grid(rows)
    m.write("SM_Dunes")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    dunes()
    aqueduct()
    triumphal_arch()
    arcade_ruin()
    temple_podium()
    obelisk()
    loose_blocks()


# ------------------------------------------------------------------ layout for Unreal
def layout():
    """Placement of every piece (world cm, degrees). Ground heights come from dune_height."""
    import json
    rng = np.random.default_rng(5)
    items = []

    def ground(x, y):
        return float(dune_height(np.array([x]), np.array([y]))[0])

    def put(mesh, x, y, yaw=0.0, scale=1.0, sink=40.0, pitch=0.0, roll=0.0, z=None, sx=None, sy=None, sz=None):
        items.append({"mesh": mesh, "x": x, "y": y, "z": (ground(x, y) - sink) if z is None else z,
                      "yaw": yaw, "pitch": pitch, "roll": roll,
                      "scale": [sx or scale, sy or scale, sz or scale]})

    def put_along(mesh, cx, cy, length, yaw, scale=1.0, sink=40.0):
        # meshes built from x=0..length: shift so (cx, cy) is the middle
        a = math.radians(yaw)
        put(mesh, cx - math.cos(a) * length * scale / 2, cy - math.sin(a) * length * scale / 2, yaw, scale, sink)

    put("SM_Dunes", 0, 0, z=0.0, sink=0)
    aq_len = 10 * 900 + 300
    put_along("SM_Aqueduct", 15000, 9500, aq_len, 120)
    put_along("SM_Aqueduct", -70000, 45000, aq_len, 25, scale=1.5, sink=150)     # silhouette on the horizon dunes
    put_along("SM_TriumphalArch", CX, 15500, 2500, 0)                           # on the road out of the gate
    put_along("SM_ArcadeRuin", -16000, -10000, 2700, 60)
    put("SM_Obelisk", 11500, -12500, yaw=12, sink=60)

    # temple: podium, a ring of columns (some whole, some broken, some gone), entablature on one corner
    tx, ty, tyaw = -16500, 9000, 10.0
    ta = math.radians(tyaw)
    put("SM_TemplePodium", tx, ty, yaw=tyaw, sink=30)
    tz = ground(tx, ty) - 30 + 300
    cs = 1.8
    def tlocal(lx, ly):
        return tx + lx * math.cos(ta) - ly * math.sin(ta), ty + lx * math.sin(ta) + ly * math.cos(ta)
    col_top = tz + 520 * cs
    for k in range(6):
        for side in (-1, 1):
            lx, ly = 200 + k * 400, side * 600
            px, py = tlocal(lx, ly)
            if side == 1 and k < 3:
                kind = "SM_RomanColumn"
            else:
                kind = rng.choice(["SM_RomanColumn", "SM_RomanColumn_BrokenA", "SM_RomanColumn_BrokenB", None], p=[0.3, 0.3, 0.25, 0.15])
            if kind:
                put(kind, px, py, yaw=tyaw + rng.uniform(-6, 6), scale=cs, z=tz)
    for k in range(3):  # architrave + frieze over the three standing corner columns
        px, py = tlocal(400 + k * 400, 600)
        put("SM_RomanBlock_A", px, py, yaw=tyaw, z=col_top, sx=4.1, sy=1.3, sz=0.9)
        put("SM_RomanBlock_B", px, py, yaw=tyaw, z=col_top + 90, sx=4.1, sy=1.2, sz=0.8)
    put("SM_RomanBlock_C", *tlocal(1650, 600), yaw=tyaw + 4, z=col_top + 170, sx=3.4, sy=1.4, sz=0.5)
    for _ in range(5):  # fallen column drums beside the temple
        px, py = tlocal(rng.uniform(-400, 2800), rng.choice([-1, 1]) * rng.uniform(900, 1500))
        put("SM_RomanColumn_BrokenB", px, py, yaw=rng.uniform(0, 360), pitch=90, scale=cs, sink=-60)

    # lone columns and scattered blocks across the sand
    for _ in range(14):
        a, d = rng.uniform(0, 2 * math.pi), rng.uniform(8000, 26000)
        put(rng.choice(["SM_RomanColumn", "SM_RomanColumn_BrokenA", "SM_RomanColumn_BrokenB"]),
            CX + d * math.cos(a), CY + d * math.sin(a), yaw=rng.uniform(0, 360), scale=rng.uniform(1.2, 1.8), sink=80,
            pitch=rng.normal(0, 4), roll=rng.normal(0, 4))
    for _ in range(60):
        a, d = rng.uniform(0, 2 * math.pi), rng.uniform(6200, 30000)
        s = rng.uniform(0.6, 1.6)
        put(rng.choice(["SM_RomanBlock_A", "SM_RomanBlock_B", "SM_RomanBlock_C"]), CX + d * math.cos(a), CY + d * math.sin(a),
            yaw=rng.uniform(0, 360), pitch=rng.normal(0, 12), roll=rng.normal(0, 12), sx=s * rng.uniform(1, 1.8), sy=s, sz=s * 0.7, sink=30 * s)

    out = os.path.join(OUT, "world_layout.json")
    json.dump({"ground": GROUND, "center": [CX, CY], "items": items}, open(out, "w"), indent=1)
    print("wrote world_layout.json with %d pieces" % len(items))


if __name__ == "__main__":
    layout()
