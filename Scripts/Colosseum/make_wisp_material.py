"""M_SandWisp: soft translucent streak for blowing sand / rain (edges fade out, fades into the ground)."""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
path = "/Game/ThirdPerson/Colosseum/M_SandWisp"

if eal.does_asset_exist(path):
    mat = eal.load_asset(path)
    mel.delete_all_material_expressions(mat)
else:
    mat = tools.create_asset("M_SandWisp", "/Game/ThirdPerson/Colosseum", unreal.Material, unreal.MaterialFactoryNew())

mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("used_with_instanced_static_meshes", True)  # it is drawn by DriftParticles' instanced mesh


def node(cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


color = node(unreal.MaterialExpressionVectorParameter, -600, -200, parameter_name="Color",
             default_value=unreal.LinearColor(0.86, 0.7, 0.48, 1))
bright = node(unreal.MaterialExpressionScalarParameter, -600, -50, parameter_name="Glow", default_value=1.0)
emis = node(unreal.MaterialExpressionMultiply, -350, -150)
mel.connect_material_expressions(color, "", emis, "A")
mel.connect_material_expressions(bright, "", emis, "B")
mel.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

# Soft edges: facing the camera = solid, silhouette = transparent
fres = node(unreal.MaterialExpressionFresnel, -800, 150, exponent=1.0)
inv = node(unreal.MaterialExpressionOneMinus, -650, 150)
mel.connect_material_expressions(fres, "", inv, "")
soft = node(unreal.MaterialExpressionPower, -500, 150, const_exponent=2.5)
mel.connect_material_expressions(inv, "", soft, "Base")
opacity = node(unreal.MaterialExpressionScalarParameter, -650, 300, parameter_name="Opacity", default_value=0.2)
m1 = node(unreal.MaterialExpressionMultiply, -350, 200)
mel.connect_material_expressions(soft, "", m1, "A")
mel.connect_material_expressions(opacity, "", m1, "B")
fade = node(unreal.MaterialExpressionDepthFade, -200, 250, fade_distance_default=120.0)
mel.connect_material_expressions(m1, "", fade, "Opacity")
mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(mat)
eal.save_loaded_asset(mat)
print("[Wisp] M_SandWisp ready")
