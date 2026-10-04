"""Play mode only: find the arena's inner wall and floor height by tracing against the colosseum."""
import math
import unreal

w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
acts = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor)
col = [a for a in acts if a.static_mesh_component.get_editor_property("static_mesh")
       and a.static_mesh_component.get_editor_property("static_mesh").get_name() == "colosseum"][0]
ignore = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor) if a != col]
CH = unreal.TraceTypeQuery.ECC_VISIBILITY


def trace(s, e):
    r = unreal.SystemLibrary.line_trace_single(w, s, e, CH, True, ignore, unreal.DrawDebugTrace.NONE, True)
    if r is None:
        return None, None
    t = r.to_tuple()
    return (t[3], t[4]) if t[0] else (None, None)


c = col.get_actor_bounds(False)[0]
print("center", round(c.x), round(c.y))
_, g = trace(unreal.Vector(c.x + 2500, c.y, 3000), unreal.Vector(c.x + 2500, c.y, -3000))
print("ground z at +25m x:", g.z if g else None)
for deg in range(0, 360, 15):
    d = unreal.Vector(math.cos(math.radians(deg)), math.sin(math.radians(deg)), 0)
    row = []
    for z in (100, 250, 400):
        s = unreal.Vector(c.x, c.y, z)
        dist, _ = trace(s, s + d * 9000)
        row.append(round(dist) if dist else None)
    print("ANG", deg, row)
