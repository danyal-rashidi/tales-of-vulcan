"""Turn the template playground into ruins (one undo step):
- delete the gray grid border walls
- rocky-sand ground
- green template blocks -> Roman columns (whole + broken), center disc/platform -> sandstone
"""
import random

import unreal

aes = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
D = "/Game/ThirdPerson/Colosseum"
ground = eal.load_asset(D + "/M_ArenaGround")
stone = eal.load_asset(D + "/MI_ColosseumSandstone")
cols = [eal.load_asset(D + "/Meshes/" + n) for n in ("SM_RomanColumn", "SM_RomanColumn_BrokenA", "SM_RomanColumn_BrokenB")]
rnd = random.Random(117)


def info(a):
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    if not c or not c.get_editor_property("static_mesh"):
        return None, None, None
    mats = c.get_materials()
    return c, c.get_editor_property("static_mesh").get_name(), (mats[0].get_name() if mats and mats[0] else "")


if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise SystemExit("[Ruins] Stop Play mode first, then run this again.")

removed = replaced = retex = 0
with unreal.ScopedEditorTransaction("Colosseum ruins makeover"):
    for a in list(aes.get_all_level_actors()):
        if str(a.get_folder_path()) != "Playground":
            continue
        c, mesh, mat = info(a)
        if c is None:
            continue
        o, e = a.get_actor_bounds(False)
        if mesh == "SM_Template_Map_Floor":
            c.set_material(0, ground)
            retex += 1
        elif mat == "MI_PrototypeGrid_Gray":
            if mesh == "SM_Cube" and max(abs(o.x), abs(o.y)) >= 1800:  # border walls
                aes.destroy_actor(a)
                removed += 1
            else:  # center platform the Vulcan boss stands on
                for i in range(c.get_num_materials()):
                    c.set_material(i, stone)
                retex += 1
        elif mat == "MI_ThirdPersonColWay":
            if e.z * 2 < 50:  # thin disc on top of the boss platform
                c.set_material(0, stone)
                retex += 1
                continue
            label = a.get_actor_label()
            # mostly whole columns, some broken, for a ruined look
            mesh_asset = rnd.choices(cols, weights=(5, 3, 2))[0]
            aes.destroy_actor(a)
            col = aes.spawn_actor_from_object(mesh_asset, unreal.Vector(o.x, o.y, 0.0),
                                              unreal.Rotator(roll=0.0, pitch=0.0, yaw=rnd.uniform(0, 360)))
            col.set_actor_label("RomanColumn_" + label)
            col.set_folder_path("Ruins/Columns")
            col.static_mesh_component.set_collision_profile_name("BlockAll")
            replaced += 1

print("[Ruins] removed %d grid walls, replaced %d green blocks with columns, retextured %d" % (removed, replaced, retex))
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
print("[Ruins] level saved")
