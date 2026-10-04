"""Import the generated Roman world and place it around the colosseum (one undo step, safe to re-run).

Run make_roman_world.py first (writes the OBJ meshes + world_layout.json).
Adds: sand dunes to the horizon, aqueducts, triumphal arch, temple ruin, arcade ruin, obelisk,
lone columns and rubble, blowing sand outside the colosseum, and blood rain that waits for
Vulcan's storm.
"""
import json
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceArt", "Colosseum")
D = "/Game/ThirdPerson/Colosseum"
WORLD = D + "/World"
PREFIX = "World_"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
aes = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

MESHES = ["SM_Dunes", "SM_Aqueduct", "SM_TriumphalArch", "SM_ArcadeRuin", "SM_TemplePodium", "SM_Obelisk",
          "SM_RomanBlock_A", "SM_RomanBlock_B", "SM_RomanBlock_C"]


def log(m):
    print("[World] " + m)


def node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def dune_material():
    """Sand projected from world XY, with a large-scale second sample to hide tiling on the dunes."""
    path = D + "/M_Dunes"
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_Dunes", D, unreal.Material, unreal.MaterialFactoryNew())
    tex = {s: eal.load_asset(D + "/Textures/T_Sand_" + s) for s in ("D", "N", "R")}
    wp = node(mat, unreal.MaterialExpressionWorldPosition, -1400, 0)
    tile = node(mat, unreal.MaterialExpressionScalarParameter, -1400, 150, parameter_name="TileSize", default_value=900.0)
    div = node(mat, unreal.MaterialExpressionDivide, -1200, 50)
    mel.connect_material_expressions(wp, "", div, "A")
    mel.connect_material_expressions(tile, "", div, "B")
    uv = node(mat, unreal.MaterialExpressionComponentMask, -1050, 50, r=True, g=True, b=False, a=False)
    mel.connect_material_expressions(div, "", uv, "")
    big = node(mat, unreal.MaterialExpressionDivide, -900, 300, const_b=11.0)
    mel.connect_material_expressions(uv, "", big, "A")

    def samp(s, stype, y, uvs):
        e = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, y, parameter_name="Sand_" + s,
                 texture=tex[s], sampler_type=stype)
        mel.connect_material_expressions(uvs, "", e, "UVs")
        return e

    d = samp("D", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -300, uv)
    dm = samp("D", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -50, big)
    lum = node(mat, unreal.MaterialExpressionDesaturation, -450, -50)
    mel.connect_material_expressions(dm, "RGB", lum, "")
    sc = node(mat, unreal.MaterialExpressionMultiply, -300, -50, const_b=1.5)
    mel.connect_material_expressions(lum, "", sc, "A")
    tint = node(mat, unreal.MaterialExpressionVectorParameter, -450, -450, parameter_name="Tint",
                default_value=unreal.LinearColor(0.95, 0.85, 0.7, 1))
    m1 = node(mat, unreal.MaterialExpressionMultiply, -250, -300)
    mel.connect_material_expressions(d, "RGB", m1, "A")
    mel.connect_material_expressions(sc, "", m1, "B")
    m2 = node(mat, unreal.MaterialExpressionMultiply, -100, -350)
    mel.connect_material_expressions(m1, "", m2, "A")
    mel.connect_material_expressions(tint, "", m2, "B")
    mel.connect_material_property(m2, "", unreal.MaterialProperty.MP_BASE_COLOR)
    n = samp("N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 200, uv)
    mel.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    r = samp("R", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 450, uv)
    mel.connect_material_property(r, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def ruin_material():
    """Weathered, sun-baked limestone for the buildings (child of the column material)."""
    path = D + "/MI_RomanRuin"
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        "MI_RomanRuin", D, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, eal.load_asset(D + "/M_RomanColumn"))
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(0.86, 0.74, 0.58, 1))
    mel.set_material_instance_scalar_parameter_value(mi, "UVScale", 1.0)
    eal.save_loaded_asset(mi)
    return mi


def import_meshes(dunes_mat, ruin_mat):
    tasks = []
    for n in MESHES:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(SRC, n + ".obj")
        t.destination_path = WORLD
        t.destination_name = n
        t.automated = True
        t.replace_existing = True
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    meshes = {}
    for n in MESHES:
        m = eal.load_asset(WORLD + "/" + n)
        if m is None:
            raise RuntimeError("import failed: " + n)
        for i in range(len(m.get_editor_property("static_materials"))):
            m.set_material(i, dunes_mat if n == "SM_Dunes" else ruin_mat)
        m.get_editor_property("body_setup").set_editor_property(
            "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        eal.save_loaded_asset(m)
        meshes[n] = m
    for n in ("SM_RomanColumn", "SM_RomanColumn_BrokenA", "SM_RomanColumn_BrokenB"):
        meshes[n] = eal.load_asset(D + "/Meshes/" + n)
    log("imported %d meshes" % len(MESHES))
    return meshes


def folder_for(mesh):
    if mesh == "SM_Dunes":
        return "World/Desert"
    if "Block" in mesh:
        return "World/Rubble"
    if "Column" in mesh:
        return "World/Columns"
    return "World/Buildings"


def spawn_weather(center, ground):
    sand = aes.spawn_actor_from_class(unreal.DriftParticles, unreal.Vector(center[0], center[1], ground + 650))
    sand.set_actor_label(PREFIX + "BlowingSand")
    sand.set_folder_path("World/Weather")
    for k, v in {
        "count": 5000, "area": unreal.Vector(24000, 24000, 1300), "follow_camera": False,
        "velocity": unreal.Vector(1700, 790, -60), "turbulence": 380.0,
        "particle_size": unreal.Vector(28, 28, 800), "size_variation": 0.5,
        "color": unreal.LinearColor(0.66, 0.51, 0.33, 1), "glow": 0.8, "roughness": 1.0,
        "custom_material": eal.load_asset(D + "/M_SandWisp"), "opacity": 0.16,
        "exclude_ellipse": unreal.Vector2D(5400, 4400), "exclude_height": 3000.0,
    }.items():
        sand.set_editor_property(k, v)

    rain = aes.spawn_actor_from_class(unreal.DriftParticles, unreal.Vector(center[0], center[1], ground + 1000))
    rain.set_actor_label(PREFIX + "BloodRain")
    rain.set_folder_path("World/Weather")
    for k, v in {
        "count": 6500, "area": unreal.Vector(5000, 5000, 2600), "follow_camera": True,
        "velocity": unreal.Vector(260, 120, -2300), "turbulence": 90.0,
        "particle_size": unreal.Vector(2.6, 2.6, 95), "size_variation": 0.35,
        "color": unreal.LinearColor(0.55, 0.015, 0.012, 1), "glow": 0.45, "roughness": 0.15,
        "wait_for_storm": True,
    }.items():
        rain.set_editor_property(k, v)
    log("weather: blowing sand outside the colosseum + blood rain (starts with the storm)")


def main():
    layout = json.load(open(os.path.join(SRC, "world_layout.json")))
    dunes_mat = dune_material()
    ruin_mat = ruin_material()
    meshes = import_meshes(dunes_mat, ruin_mat)

    with unreal.ScopedEditorTransaction("Roman world"):
        for a in aes.get_all_level_actors():
            if a.get_actor_label().startswith(PREFIX):
                aes.destroy_actor(a)
        for i, it in enumerate(layout["items"]):
            mesh = meshes[it["mesh"]]
            actor = aes.spawn_actor_from_object(mesh, unreal.Vector(it["x"], it["y"], it["z"]),
                                                unreal.Rotator(roll=it["roll"], pitch=it["pitch"], yaw=it["yaw"]))
            actor.set_actor_scale3d(unreal.Vector(*it["scale"]))
            actor.set_actor_label("%s%s_%03d" % (PREFIX, it["mesh"].replace("SM_", ""), i))
            actor.set_folder_path(folder_for(it["mesh"]))
            actor.static_mesh_component.set_collision_profile_name("BlockAll")
        spawn_weather(layout["center"], layout["ground"])

    log("placed %d pieces" % len(layout["items"]))
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("saved")


main()
