"""Import the generated column meshes + textures and build their materials."""
import os

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT_DIR, "SourceArt", "Colosseum")
DEST = "/Game/ThirdPerson/Colosseum"
TEX = DEST + "/Textures"
MESHES = DEST + "/Meshes"

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(m):
    print("[Ruins] " + m)


def import_files(names, dest):
    tasks = []
    for fn in names:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(SRC, fn)
        t.destination_path = dest
        t.destination_name = os.path.splitext(fn)[0]
        t.automated = True
        t.replace_existing = True
        t.save = False
        tasks.append(t)
    tools.import_asset_tasks(tasks)


def textures():
    names = ["T_Limestone_D", "T_Limestone_N", "T_Limestone_R", "T_RockySand_D", "T_RockySand_N", "T_RockySand_R"]
    import_files([n + ".png" for n in names], TEX)
    out = {}
    for n in names:
        t = eal.load_asset(TEX + "/" + n)
        if n.endswith("_N"):
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            t.set_editor_property("srgb", False)
        elif n.endswith("_R"):
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            t.set_editor_property("srgb", False)
        eal.save_loaded_asset(t)
        out[n] = t
    log("textures imported")
    return out


def node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def simple_material(name, prefix, tex, world_xy, tile_default):
    """Base color/normal/roughness from one texture set.
    world_xy=True projects from world XY (ground); otherwise uses mesh UVs."""
    path = DEST + "/" + name
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    tile = node(mat, unreal.MaterialExpressionScalarParameter, -1300, 0, parameter_name="TileSize" if world_xy else "UVScale",
                default_value=tile_default)
    if world_xy:
        wp = node(mat, unreal.MaterialExpressionWorldPosition, -1300, -150)
        div = node(mat, unreal.MaterialExpressionDivide, -1100, -100)
        mel.connect_material_expressions(wp, "", div, "A")
        mel.connect_material_expressions(tile, "", div, "B")
        uv = node(mat, unreal.MaterialExpressionComponentMask, -950, -100, r=True, g=True, b=False, a=False)
        mel.connect_material_expressions(div, "", uv, "")
        # second, much larger projection to break up visible tiling
        div2 = node(mat, unreal.MaterialExpressionDivide, -1100, 250, const_b=7.3)
        mel.connect_material_expressions(uv, "", div2, "A")
        uv_macro = div2
    else:
        tc = node(mat, unreal.MaterialExpressionTextureCoordinate, -1300, -150)
        uv = node(mat, unreal.MaterialExpressionMultiply, -1100, -100)
        mel.connect_material_expressions(tc, "", uv, "A")
        mel.connect_material_expressions(tile, "", uv, "B")
        uv_macro = None

    def samp(suffix, stype, y, uvs, pname=None):
        s = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, y,
                 parameter_name=pname or (prefix + "_" + suffix), texture=tex["T_%s_%s" % (prefix, suffix)], sampler_type=stype)
        mel.connect_material_expressions(uvs, "", s, "UVs")
        return s

    d = samp("D", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -400, uv)
    tint = node(mat, unreal.MaterialExpressionVectorParameter, -700, -600, parameter_name="Tint",
                default_value=unreal.LinearColor(1, 1, 1, 1))
    col = node(mat, unreal.MaterialExpressionMultiply, -350, -400)
    mel.connect_material_expressions(d, "RGB", col, "A")
    mel.connect_material_expressions(tint, "", col, "B")
    if uv_macro is not None:
        dm = samp("D", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -150, uv_macro)
        lum = node(mat, unreal.MaterialExpressionDesaturation, -450, -150)
        mel.connect_material_expressions(dm, "RGB", lum, "")
        scale = node(mat, unreal.MaterialExpressionMultiply, -300, -150, const_b=1.6)
        mel.connect_material_expressions(lum, "", scale, "A")
        col2 = node(mat, unreal.MaterialExpressionMultiply, -150, -300)
        mel.connect_material_expressions(col, "", col2, "A")
        mel.connect_material_expressions(scale, "", col2, "B")
        col = col2
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    n = samp("N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 100, uv)
    mel.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    r = samp("R", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 350, uv)
    mel.connect_material_property(r, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = node(mat, unreal.MaterialExpressionConstant, -350, 600, r=0.3)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("material " + name)
    return mat


def meshes(col_mat):
    names = ["SM_RomanColumn", "SM_RomanColumn_BrokenA", "SM_RomanColumn_BrokenB"]
    import_files([n + ".obj" for n in names], MESHES)
    for n in names:
        m = eal.load_asset(MESHES + "/" + n)
        if m is None:
            raise RuntimeError("mesh import failed: " + n)
        for i in range(len(m.get_editor_property("static_materials"))):
            m.set_material(i, col_mat)
        m.get_editor_property("body_setup").set_editor_property(
            "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        eal.save_loaded_asset(m)
        b = m.get_bounding_box()
        log("%s bounds min=(%.0f,%.0f,%.0f) max=(%.0f,%.0f,%.0f)" % (n, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))


tex = textures()
col_mat = simple_material("M_RomanColumn", "Limestone", tex, world_xy=False, tile_default=1.0)
simple_material("M_ArenaGround", "RockySand", tex, world_xy=True, tile_default=600.0)
meshes(col_mat)
for p in eal.list_assets(MESHES, recursive=False):
    print("  asset", p)
