# Creates /Game/Vulcan/M_VulcanShape: a simple material with Color, Metallic, Roughness
# and Glow parameters, used by Vulcan's otter body and the lava projectile.
#
# Run with Unreal closed:
#   UnrealEditor-Cmd.exe <path>\TalesofVulcan.uproject -run=pythonscript
#       -script=<path>\Tools\make_vulcan_material.py -EnablePlugins=PythonScriptPlugin -unattended
import unreal

FOLDER = "/Game/Vulcan"
NAME = "M_VulcanShape"
FULL = f"{FOLDER}/{NAME}"

assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

if assets.does_asset_exist(FULL):
    assets.delete_asset(FULL)

mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def param(cls, name, default, x, y):
    node = mel.create_material_expression(mat, cls, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    return node


color = param(unreal.MaterialExpressionVectorParameter, "Color", unreal.LinearColor(1, 1, 1, 1), -600, -200)
metallic = param(unreal.MaterialExpressionScalarParameter, "Metallic", 0.8, -600, 0)
roughness = param(unreal.MaterialExpressionScalarParameter, "Roughness", 0.25, -600, 100)
glow = param(unreal.MaterialExpressionScalarParameter, "Glow", 0.0, -600, 250)

emissive = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 250)
mel.connect_material_expressions(color, "", emissive, "A")
mel.connect_material_expressions(glow, "", emissive, "B")

mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

mel.recompile_material(mat)
assets.save_asset(FULL)
unreal.log(f"Created {FULL}")
