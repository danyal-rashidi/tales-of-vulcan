"""Generate tileable sandstone-ruin and sand textures for the colosseum.

Writes 2048x2048 PNGs to SourceArt/Colosseum/:
  T_Sandstone_D  - weathered sandstone ashlar blocks (base color)
  T_Sandstone_N  - normal map (DirectX/Unreal green-down convention)
  T_Sandstone_R  - roughness
  T_Sand_D / T_Sand_N / T_Sand_R - wind-rippled arena sand
"""
import os

import numpy as np
from PIL import Image

SIZE = 2048
OUT = os.path.join(os.path.dirname(__file__), "..", "..", "SourceArt", "Colosseum")
rng = np.random.default_rng(1971)


def tile_noise(size, cells, octaves=5, persistence=0.5):
    """Tileable fractal value noise in [0, 1]."""
    out = np.zeros((size, size), np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        n = cells * (2 ** o)
        grid = rng.random((n, n)).astype(np.float32)
        coords = np.arange(size, dtype=np.float32) * n / size
        i0 = np.floor(coords).astype(int) % n
        i1 = (i0 + 1) % n
        f = coords - np.floor(coords)
        f = f * f * (3 - 2 * f)
        top = grid[i0][:, i0] * (1 - f)[None, :] + grid[i0][:, i1] * f[None, :]
        bot = grid[i1][:, i0] * (1 - f)[None, :] + grid[i1][:, i1] * f[None, :]
        out += amp * (top * (1 - f)[:, None] + bot * f[:, None])
        total += amp
        amp *= persistence
    return out / total


def blur(a, r):
    """Cheap wrap-around box blur (3 passes ~ gaussian)."""
    for _ in range(3):
        for axis in (0, 1):
            acc = np.zeros_like(a)
            for d in range(-r, r + 1):
                acc += np.roll(a, d, axis=axis)
            a = acc / (2 * r + 1)
    return a


def normal_from_height(h, strength):
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * strength
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * strength
    n = np.stack([-dx, dy, np.ones_like(h)], -1)  # +dy: DirectX green for Unreal
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return ((n * 0.5 + 0.5) * 255).astype(np.uint8)


def save(name, arr):
    os.makedirs(OUT, exist_ok=True)
    if arr.dtype != np.uint8:
        arr = (np.clip(arr, 0, 1) * 255).astype(np.uint8)
    Image.fromarray(arr).save(os.path.join(OUT, name + ".png"))
    print("wrote", name)


def sandstone():
    rows, cols = 6, 3  # large ashlar blocks, running bond
    y = np.arange(SIZE)[:, None].repeat(SIZE, 1).astype(np.float32)
    x = np.arange(SIZE)[None, :].repeat(SIZE, 0).astype(np.float32)
    bh, bw = SIZE / rows, SIZE / cols
    row = np.floor(y / bh).astype(int)
    xo = (x + (row % 2) * bw * 0.5) % SIZE
    col = np.floor(xo / bw).astype(int)
    fy, fx = (y % bh) / bh, (xo % bw) / bw

    # Worn, irregular mortar joints: edge distance perturbed by noise
    warp = tile_noise(SIZE, 8, 4) - 0.5
    edge = np.minimum(np.minimum(fx, 1 - fx) * bw, np.minimum(fy, 1 - fy) * bh)
    chips = np.clip((tile_noise(SIZE, 24, 4) - 0.55) * 4, 0, 1)  # broken corners/edges
    edge = edge + warp * 40 - chips * 45
    joint = np.clip(edge / 14.0, 0, 1)  # 0 in the joint, 1 on the face
    bevel = np.clip(edge / 55.0, 0, 1) ** 0.6

    block_id = (row * 7 + col * 13) % 97
    block_tone = (np.sin(block_id * 12.9898) * 43758.5453) % 1.0  # per-block variation

    strata = tile_noise(SIZE, 2, 3)
    strata = 0.5 + 0.5 * np.sin((y / SIZE) * np.pi * 2 * 11 + strata * 18 + block_tone * 6)  # sedimentary bands
    stains = tile_noise(SIZE, 2, 5, 0.6)  # large weathering / water-run stains
    streaks = blur(tile_noise(SIZE, 32, 2), 1)
    streaks = np.clip((np.roll(streaks, 0, 0) + blur(streaks, 6)) - 0.9, 0, 1)
    pits = tile_noise(SIZE, 64, 3)
    pits = np.clip((pits - 0.62) * 6, 0, 1)  # erosion pock marks
    grain = tile_noise(SIZE, 128, 2)
    cracks = np.abs(tile_noise(SIZE, 6, 5) - 0.5)
    cracks = np.clip(1 - cracks * 60, 0, 1) * (tile_noise(SIZE, 4, 2) > 0.55)
    large = tile_noise(SIZE, 3, 4)

    height = bevel * (0.75 + 0.12 * large + 0.06 * grain) - pits * 0.12 - cracks * 0.25
    height = blur(height, 1)

    # Warm sandstone palette: pale cream -> ochre -> rust, with soot/dirt in recesses
    pale = np.array([0.86, 0.76, 0.60])
    ochre = np.array([0.74, 0.58, 0.40])
    rust = np.array([0.60, 0.42, 0.28])
    dirt = np.array([0.36, 0.29, 0.22])
    t = np.clip(0.3 * block_tone + 0.35 * large + 0.07 * strata + 0.1 * grain + 0.35 * (stains - 0.5), 0, 1)[..., None]
    col_ = np.where(t < 0.5, pale + (ochre - pale) * (t / 0.5), ochre + (rust - ochre) * ((t - 0.5) / 0.5))
    ao = np.clip(0.55 + 0.45 * bevel, 0, 1)[..., None]
    col_ = col_ * ao
    recess = np.clip(1 - joint + pits * 0.6 + cracks * 0.8, 0, 1)[..., None]
    col_ = col_ * (1 - recess * 0.55) + dirt * recess * 0.55
    col_ *= (0.92 + 0.16 * grain)[..., None]
    grime = np.clip((stains - 0.55) * 2.5 + streaks * 0.5, 0, 1)[..., None] * 0.45
    col_ = col_ * (1 - grime) + dirt * grime

    rough = np.clip(0.78 + 0.15 * (1 - joint) + 0.05 * grain - 0.08 * large, 0, 1)

    save("T_Sandstone_D", col_)
    save("T_Sandstone_N", normal_from_height(height, 6.0))
    save("T_Sandstone_R", rough)


def sand():
    y = np.arange(SIZE)[:, None].repeat(SIZE, 1).astype(np.float32)
    warp = tile_noise(SIZE, 4, 4)
    ripples = 0.5 + 0.5 * np.sin((y / SIZE) * np.pi * 2 * 22 + warp * 14)
    ripples = ripples ** 1.6
    grain = tile_noise(SIZE, 256, 2)
    patches = tile_noise(SIZE, 3, 4)
    height = 0.6 * ripples * (0.5 + 0.5 * patches) + 0.25 * grain + 0.15 * tile_noise(SIZE, 16, 3)
    height = blur(height, 2)

    light = np.array([0.84, 0.72, 0.52])
    dark = np.array([0.66, 0.52, 0.36])
    t = np.clip(0.6 * patches + 0.25 * (1 - ripples) + 0.15 * grain, 0, 1)[..., None]
    col_ = light + (dark - light) * t
    speck = (tile_noise(SIZE, 512, 1) > 0.82)[..., None]
    col_ = np.where(speck, col_ * 0.8, col_)

    save("T_Sand_D", col_)
    save("T_Sand_N", normal_from_height(height, 3.0))
    save("T_Sand_R", np.clip(0.88 + 0.1 * grain, 0, 1))


if __name__ == "__main__":
    sandstone()
    sand()
