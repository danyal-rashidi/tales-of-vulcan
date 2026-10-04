"""Ring of Roman columns just inside the arena wall (one undo step). Safe to re-run.

Arena wall measured with measure_arena.py: ellipse ~3294 x 2612 cm around the colosseum
center, ground at z -71, gate opening at +Y (90 degrees).
"""
import math
import random

import unreal

CENTER = (-44.0, -5.0)
WALL_A, WALL_B = 3294.0, 2612.0   # inner wall semi-axes (cm)
INSET = 250.0                     # distance from the wall
COUNT = 28
GROUND_Z = -71.0                  # arena sand
SLAB_HALF = 2000.0                # the 40 x 40 m floor slab at the center, top at z 0
GATE_DEG, GATE_GAP = 90.0, 9.0    # leave the gate clear
PREFIX, FOLDER = "PerimeterColumn_", "Ruins/PerimeterColumns"

aes = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
D = "/Game/ThirdPerson/Colosseum/Meshes/"
meshes = [unreal.load_asset(D + n) for n in ("SM_RomanColumn", "SM_RomanColumn_BrokenA", "SM_RomanColumn_BrokenB")]
rnd = random.Random(2024)

a, b = WALL_A - INSET, WALL_B - INSET

# Evenly spaced by arc length around the ellipse
samples = 2000
pts = [(a * math.cos(2 * math.pi * i / samples), b * math.sin(2 * math.pi * i / samples)) for i in range(samples + 1)]
arc = [0.0]
for i in range(1, len(pts)):
    arc.append(arc[-1] + math.dist(pts[i - 1], pts[i]))
step = arc[-1] / COUNT


def on_slab(x, y):
    return abs(x) + 60 <= SLAB_HALF and abs(y) + 60 <= SLAB_HALF


placed = 0
with unreal.ScopedEditorTransaction("Perimeter columns"):
    for actor in aes.get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            aes.destroy_actor(actor)

    j = 0
    for k in range(COUNT):
        target = (k + 0.5) * step
        while arc[j] < target:
            j += 1
        px, py = pts[j]
        ang = math.degrees(math.atan2(py / b, px / a)) % 360
        if abs(ang - GATE_DEG) < GATE_GAP:
            continue
        x, y = CENTER[0] + px, CENTER[1] + py
        z = 0.0 if on_slab(x, y) else GROUND_Z   # columns at the slab edge sink into it, like buried ruins
        yaw = math.degrees(math.atan2(CENTER[1] - y, CENTER[0] - x)) + rnd.uniform(-8, 8)
        mesh = rnd.choices(meshes, weights=(6, 2, 2))[0]
        col = aes.spawn_actor_from_object(mesh, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        col.set_actor_label("%s%02d" % (PREFIX, k))
        col.set_folder_path(FOLDER)
        col.static_mesh_component.set_collision_profile_name("BlockAll")
        placed += 1

print("[Ruins] placed %d perimeter columns" % placed)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
print("[Ruins] saved")
