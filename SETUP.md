# Vulcan, the Silver Tide — setup

## Files

| File | What it is |
|---|---|
| `HealthComponent.h/.cpp` | Health for anything (Vulcan, player). Works with Unreal's "Apply Damage". |
| `VulcanProjectile.h/.cpp` | Obsidian shard for the Spit attack. |
| `VulcanBoss.h/.cpp` | The boss: AI, Tail Lash, Obsidian Spit, Molten Breath, Magma Dive, Phase 2. |

## 1. Copy the code into the project

The project must be a **C++ project** (or have had a C++ class added via Tools → New C++ Class).

1. Close Unreal.
2. Copy the 6 files from `Source/TalesofVulcan/` into the repo's `Source/<ProjectName>/` folder
   (next to `<ProjectName>.Build.cs`).
3. **If the project is NOT named `TalesofVulcan`:** in all 3 `.h` files, replace
   `TALESOFVULCAN_API` with `<PROJECTNAME>_API` (all caps, e.g. `OTTERGAME_API`).
4. Open `Source/<ProjectName>/<ProjectName>.Build.cs` and add `"AIModule", "GameplayTasks"`
   to the `PublicDependencyModuleNames` list, e.g.:
   ```csharp
   PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "GameplayTasks" });
   ```
5. Right-click the `.uproject` → **Generate Visual Studio project files**.
6. Double-click the `.uproject` to open Unreal. Click **Yes** when it asks to rebuild.
   If the build fails, copy the error text and send it to Claude.

## 2. Make the Blueprint

1. Content Browser → right-click → **Blueprint Class** → search **VulcanBoss** → name it `BP_Vulcan`.
2. Open it. Select **Mesh** → set Skeletal Mesh (mannequin for now, otter later) and Anim Class.
   Scale the whole actor up (e.g. 1.8) in the level, or the capsule + mesh in the Blueprint.
3. In the **Details** panel, under **Vulcan**, assign montages (any are optional; empty = no animation,
   the attacks still work):
   Tail Lash, Spit, Breath, Dive, Emerge, Death.
   *To make a montage:* right-click an animation → Create → Create AnimMontage.
   **The Anim Blueprint must have a `DefaultSlot` node** or montages won't play (the template's does).
4. Drag `BP_Vulcan` into the level.

## 3. Level setup

- Add a **Nav Mesh Bounds Volume** (Place Actors panel) covering the arena floor. Press **P** to
  see it — green floor = Vulcan can walk there. **Without this Vulcan won't move.**

## 4. Player setup (whoever owns the player Blueprint)

- Open the player Blueprint → **Add Component** → **Health Component**. Set MaxHealth (e.g. 100).
- Dodge i-frames: at the start of the roll set `Health Component → Invulnerable = true`,
  at the end set it back to `false`.
- Player attacks: on hit, call **Apply Damage** with Vulcan as the Damaged Actor
  (e.g. 20 damage). Vulcan's health drops automatically.
- Player death: bind **On Death** of the player's Health Component → show death screen / restart.

## 5. Visuals & UI (in BP_Vulcan's Event Graph)

Right-click → search "Event On ..." to add these:

| Event | Hook up |
|---|---|
| On Fight Started | Create boss health bar widget, add to viewport |
| On Breath Started / Ended | Activate / Deactivate a Niagara fire component on the mouth |
| On Breath Windup | Glow / charge sound |
| On Dive Warning (Location, Radius) | Spawn a glowing decal at Location |
| On Dive Submerged / Emerged | Lava splash effects, camera shake |
| On Burn Patch Started | Fire effect at Location for Duration |
| On Phase Two Started | Brighter material, roar, change health bar text |
| On Vulcan Defeated | "PROJECT SUBMITTED" screen |

**Health bar:** in the widget, bind the progress bar percent to
`BP_Vulcan → Health Component → Get Health Percent`.

## 6. Tuning

Everything is editable in BP_Vulcan's Details under **Vulcan|...**: damage, ranges, timings,
cooldown, Phase 2 multipliers.

- **Show Debug** (on by default) draws hitboxes: orange cone = tail lash, red cone = flames,
  circles = dive / burn patch. Turn it off for the demo.
- Running short on time? Untick **Enable Dive** to cut Magma Dive.
- **Start Fight On Begin Play** off + a Box Trigger calling **Start Fight** = fight starts when
  the player walks into the arena.
