"""Colosseum setup: sandstone-ruin material + collision + invisible perimeter wall.

Run inside the Unreal Editor (Output Log, "Cmd" box):
    py "C:/Users/ddodg/Desktop/tales-of-vulcan 5.8 - 3/Scripts/Colosseum/setup_colosseum.py"

Safe to re-run: textures/material are replaced in place and the perimeter
wall actors are deleted and rebuilt each time.
"""
import math
import os

import unreal

PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE_ART = os.path.join(PROJECT_DIR, "SourceArt", "Colosseum")
MESH_PATH = "/Game/ThirdPerson/colosseum"
DEST = "/Game/ThirdPerson/Colosseum"
TEX_DEST = DEST + "/Textures"

WALL_FOLDER = "Colosseum/PerimeterCollision"
WALL_PREFIX = "ColosseumPerimeterWall"
WALL_SEGMENTS = 64
WALL_THICKNESS = 60.0  # cm
WALL_EXTRA_HEIGHT = 600.0  # cm above the top of the colosseum, so it can't be jumped
WALL_RADIUS_SCALE = 1.01  # just outside the outer facade

mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("[Colosseum] " + msg)


# ---------------------------------------------------------------- textures
def import_textures():
    tasks = []
    for name in ("T_Sandstone_D", "T_Sandstone_N", "T_Sandstone_R", "T_Sand_D", "T_Sand_N", "T_Sand_R"):
        t = unreal.AssetImportTask()
        t.filename = os.path.join(SOURCE_ART, name + ".png")
        t.destination_path = TEX_DEST
        t.destination_name = name
        t.automated = True
        t.replace_existing = True
        t.save = False
        tasks.append(t)
    asset_tools.import_asset_tasks(tasks)

    textures = {}
    for name in ("T_Sandstone_D", "T_Sandstone_N", "T_Sandstone_R", "T_Sand_D", "T_Sand_N", "T_Sand_R"):
        tex = eal.load_asset(TEX_DEST + "/" + name)
        if tex is None:
            raise RuntimeError("Texture import failed: " + name)
        if name.endswith("_N"):
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("flip_green_channel", False)
        elif name.endswith("_R"):
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property("srgb", False)
        eal.save_loaded_asset(tex)
        textures[name] = tex
    log("Imported %d textures" % len(textures))
    return textures


# ---------------------------------------------------------------- material
def node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(src, dst, dst_input="", src_output=""):
    mel.connect_material_expressions(src, src_output, dst, dst_input)


def build_material(textures):
    path = DEST + "/M_ColosseumSandstone"
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset("M_ColosseumSandstone", DEST, unreal.Material, unreal.MaterialFactoryNew())

    E = unreal  # shorthand for expression classes

    # World-space projections (triplanar): mesh UVs are a non-tiling unwrap, so
    # project tiling textures from world position instead.
    wp = node(mat, E.MaterialExpressionWorldPosition, -2200, 0)
    stone_size = node(mat, E.MaterialExpressionScalarParameter, -2200, 150, parameter_name="StoneTileSize", default_value=450.0)
    sand_size = node(mat, E.MaterialExpressionScalarParameter, -2200, 250, parameter_name="SandTileSize", default_value=350.0)

    def projections(size, y):
        div = node(mat, E.MaterialExpressionDivide, -2000, y)
        link(wp, div, "A")
        link(size, div, "B")
        yz = node(mat, E.MaterialExpressionComponentMask, -1850, y - 60, r=False, g=True, b=True, a=False)
        xz = node(mat, E.MaterialExpressionComponentMask, -1850, y, r=True, g=False, b=True, a=False)
        xy = node(mat, E.MaterialExpressionComponentMask, -1850, y + 60, r=True, g=True, b=False, a=False)
        for m in (yz, xz, xy):
            link(div, m)
        return yz, xz, xy

    s_yz, s_xz, s_xy = projections(stone_size, 0)
    _, _, d_xy = projections(sand_size, 400)

    # Blend masks from the world-space vertex normal
    vn = node(mat, E.MaterialExpressionVertexNormalWS, -2200, 800)
    nx = node(mat, E.MaterialExpressionComponentMask, -2000, 760, r=True, g=False, b=False, a=False)
    ny = node(mat, E.MaterialExpressionComponentMask, -2000, 820, r=False, g=True, b=False, a=False)
    nz = node(mat, E.MaterialExpressionComponentMask, -2000, 880, r=False, g=False, b=True, a=False)
    for m in (nx, ny, nz):
        link(vn, m)
    ax = node(mat, E.MaterialExpressionAbs, -1850, 760)
    ay = node(mat, E.MaterialExpressionAbs, -1850, 820)
    link(nx, ax)
    link(ny, ay)
    sub = node(mat, E.MaterialExpressionSubtract, -1700, 780)
    link(ax, sub, "A")
    link(ay, sub, "B")
    mul = node(mat, E.MaterialExpressionMultiply, -1550, 780, const_b=4.0)
    link(sub, mul, "A")
    add = node(mat, E.MaterialExpressionAdd, -1400, 780, const_b=0.5)
    link(mul, add, "A")
    wall_mask = node(mat, E.MaterialExpressionSaturate, -1250, 780)
    link(add, wall_mask)

    top_thresh = node(mat, E.MaterialExpressionScalarParameter, -2000, 960, parameter_name="SandSlopeThreshold", default_value=0.6)
    zsub = node(mat, E.MaterialExpressionSubtract, -1700, 900)
    link(nz, zsub, "A")
    link(top_thresh, zsub, "B")
    zmul = node(mat, E.MaterialExpressionMultiply, -1550, 900, const_b=6.0)
    link(zsub, zmul, "A")
    top_mask = node(mat, E.MaterialExpressionSaturate, -1250, 900)
    link(zmul, top_mask)

    sand_amount = node(mat, E.MaterialExpressionScalarParameter, -1250, 1000, parameter_name="SandOnFloors", default_value=0.85)

    def sampler(param, tex, uv, x, y, stype):
        s = node(mat, E.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=param, texture=tex, sampler_type=stype)
        link(uv, s, "UVs")
        return s

    def channel(suffix, stype, out, y):
        stone_tex = textures["T_Sandstone_" + suffix]
        sand_tex = textures["T_Sand_" + suffix]
        sx = sampler("Stone_" + suffix, stone_tex, s_yz, -1100, y, stype)
        sy = sampler("Stone_" + suffix, stone_tex, s_xz, -1100, y + 220, stype)
        sz = sampler("Stone_" + suffix, stone_tex, s_xy, -1100, y + 440, stype)
        dz = sampler("Sand_" + suffix, sand_tex, d_xy, -1100, y + 660, stype)
        wall = node(mat, E.MaterialExpressionLinearInterpolate, -750, y + 100)
        link(sy, wall, "A", out)
        link(sx, wall, "B", out)
        link(wall_mask, wall, "Alpha")
        top = node(mat, E.MaterialExpressionLinearInterpolate, -750, y + 500)
        link(sz, top, "A", out)
        link(dz, top, "B", out)
        link(sand_amount, top, "Alpha")
        final = node(mat, E.MaterialExpressionLinearInterpolate, -550, y + 300)
        link(wall, final, "A")
        link(top, final, "B")
        link(top_mask, final, "Alpha")
        return final

    color = channel("D", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "RGB", -1600)
    tint = node(mat, E.MaterialExpressionVectorParameter, -550, -1700, parameter_name="Tint",
                default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    tinted = node(mat, E.MaterialExpressionMultiply, -300, -1500)
    link(color, tinted, "A")
    link(tint, tinted, "B")
    mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal = channel("N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, "RGB", -700)
    flat = node(mat, E.MaterialExpressionConstant3Vector, -550, -800, constant=unreal.LinearColor(0, 0, 1, 1))
    nstr = node(mat, E.MaterialExpressionScalarParameter, -550, -700, parameter_name="NormalStrength", default_value=1.0)
    nlerp = node(mat, E.MaterialExpressionLinearInterpolate, -300, -500)
    link(flat, nlerp, "A")
    link(normal, nlerp, "B")
    link(nstr, nlerp, "Alpha")
    mel.connect_material_property(nlerp, "", unreal.MaterialProperty.MP_NORMAL)

    rough = channel("R", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, "R", 200)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = node(mat, E.MaterialExpressionConstant, -300, 600, r=0.3)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)

    mi_path = DEST + "/MI_ColosseumSandstone"
    if eal.does_asset_exist(mi_path):
        mi = eal.load_asset(mi_path)
    else:
        mi = asset_tools.create_asset("MI_ColosseumSandstone", DEST, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    eal.save_loaded_asset(mi)
    log("Built M_ColosseumSandstone + MI_ColosseumSandstone")
    return mi


# ---------------------------------------------------------------- mesh: material + collision
def setup_mesh(mesh, mi):
    for i in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(i, mi)

    # Per-poly collision that follows the real walls, stands and arches
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(mesh)
    log("Mesh: material assigned, collision = Use Complex As Simple")


# ---------------------------------------------------------------- level actors
def colosseum_actors():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    found = []
    for a in actors:
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            sm = c.get_editor_property("static_mesh")
            if sm and sm.get_path_name().startswith(MESH_PATH + "."):
                found.append((a, c))
    return found


def build_perimeter(actor, comp, mesh, cube):
    aes = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    box = mesh.get_bounding_box()
    mn, mx = box.min, box.max
    cx, cy = (mn.x + mx.x) / 2, (mn.y + mx.y) / 2
    rx = (mx.x - mn.x) / 2 * WALL_RADIUS_SCALE
    ry = (mx.y - mn.y) / 2 * WALL_RADIUS_SCALE
    xf = comp.get_world_transform()

    def world(t, z):
        return xf.transform_location(unreal.Vector(cx + rx * math.cos(t), cy + ry * math.sin(t), z))

    bottom = world(0, mn.z).z
    top = world(0, mx.z).z + WALL_EXTRA_HEIGHT
    height = top - bottom

    for i in range(WALL_SEGMENTS):
        t0 = 2 * math.pi * i / WALL_SEGMENTS
        t1 = 2 * math.pi * (i + 1) / WALL_SEGMENTS
        p0, p1 = world(t0, mn.z), world(t1, mn.z)
        dx, dy = p1.x - p0.x, p1.y - p0.y
        length = math.hypot(dx, dy) + WALL_THICKNESS  # overlap at joints, no gaps
        loc = unreal.Vector((p0.x + p1.x) / 2, (p0.y + p1.y) / 2, bottom + height / 2)
        rot = unreal.Rotator(roll=0.0, pitch=0.0, yaw=math.degrees(math.atan2(dy, dx)))
        wall = aes.spawn_actor_from_object(cube, loc, rot)
        wall.set_actor_scale3d(unreal.Vector(length / 100, WALL_THICKNESS / 100, height / 100))
        wall.set_actor_label("%s_%02d" % (WALL_PREFIX, i))
        wall.set_folder_path(WALL_FOLDER)
        wall.set_actor_hidden_in_game(True)
        smc = wall.static_mesh_component
        smc.set_visibility(False)  # still blocks; invisible in editor and game
        smc.set_editor_property("cast_shadow", False)
        smc.set_collision_profile_name("InvisibleWall")
    log("Perimeter: %d invisible wall segments around %s (%.0f x %.0f m)"
        % (WALL_SEGMENTS, actor.get_actor_label(),
           (world(0, mn.z) - world(math.pi, mn.z)).length() / 100,
           (world(math.pi / 2, mn.z) - world(-math.pi / 2, mn.z)).length() / 100))


def setup_level(mesh, mi):
    aes = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in aes.get_all_level_actors():
        if a.get_actor_label().startswith(WALL_PREFIX):
            aes.destroy_actor(a)

    placed = colosseum_actors()
    if not placed:
        unreal.log_warning("[Colosseum] No colosseum actor is loaded in this level. "
                           "Load its World Partition region (or place it) and run again for the perimeter wall.")
        return
    cube = eal.load_asset("/Engine/BasicShapes/Cube")
    for actor, comp in placed:
        for i in range(comp.get_num_materials()):
            comp.set_material(i, mi)
        comp.set_collision_profile_name("BlockAll")
        build_perimeter(actor, comp, mesh, cube)

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("Level saved")


def main():
    mesh = eal.load_asset(MESH_PATH)
    if mesh is None:
        raise RuntimeError("Could not find " + MESH_PATH)
    textures = import_textures()
    mi = build_material(textures)
    setup_mesh(mesh, mi)
    setup_level(mesh, mi)
    log("Done.")


main()
