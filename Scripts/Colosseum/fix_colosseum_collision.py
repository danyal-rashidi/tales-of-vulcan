"""Give the placed colosseum real collision (BlockAll), recorded so it is saved with the level."""
import unreal

aes = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
with unreal.ScopedEditorTransaction("Colosseum collision"):
    for a in aes.get_all_level_actors():
        if not isinstance(a, unreal.StaticMeshActor):
            continue
        c = a.static_mesh_component
        sm = c.get_editor_property("static_mesh")
        if sm and sm.get_name() == "colosseum":
            bi = c.get_editor_property("body_instance")
            bi.set_editor_property("collision_profile_name", "BlockAll")
            bi.set_editor_property("collision_enabled", unreal.CollisionEnabled.QUERY_AND_PHYSICS)
            c.set_editor_property("body_instance", bi)
            print("[Fix]", a.get_actor_label(), c.get_collision_profile_name(), c.get_collision_enabled())
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
print("[Fix] saved")
