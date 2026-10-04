"""Re-import fixed column meshes and make the floor/platform materials stick (marks actors dirty)."""
import os
import unreal

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
SRC = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Colosseum")
D = "/Game/ThirdPerson/Colosseum"

tasks = []
for n in ("SM_RomanColumn", "SM_RomanColumn_BrokenA", "SM_RomanColumn_BrokenB"):
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, n + ".obj")
    t.destination_path = D + "/Meshes"
    t.destination_name = n
    t.automated = True
    t.replace_existing = True
    tasks.append(t)
tools.import_asset_tasks(tasks)
col_mat = eal.load_asset(D + "/M_RomanColumn")
for n in ("SM_RomanColumn", "SM_RomanColumn_BrokenA", "SM_RomanColumn_BrokenB"):
    m = eal.load_asset(D + "/Meshes/" + n)
    for i in range(len(m.get_editor_property("static_materials"))):
        m.set_material(i, col_mat)
    m.get_editor_property("body_setup").set_editor_property(
        "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(m)
print("[Fix] columns re-imported")

ground = eal.load_asset(D + "/M_ArenaGround")
stone = eal.load_asset(D + "/MI_ColosseumSandstone")
aes = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
n = 0
with unreal.ScopedEditorTransaction("Ruins materials"):
    for a in aes.get_all_level_actors():
        if str(a.get_folder_path()) != "Playground":
            continue
        c = a.get_component_by_class(unreal.StaticMeshComponent)
        if not c or not c.get_editor_property("static_mesh"):
            continue
        mat = ground if c.get_editor_property("static_mesh").get_name() == "SM_Template_Map_Floor" else stone
        # set_editor_property records the change so it is saved (set_material alone was not)
        c.set_editor_property("override_materials", [mat] * max(1, c.get_num_materials()))
        n += 1
print("[Fix] retextured %d playground actors" % n)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
print("[Fix] saved")
