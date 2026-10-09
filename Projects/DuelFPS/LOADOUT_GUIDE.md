# DuelFPS armory, loadout presets and GunPack viewmodels

## Controls

- **Main Menu > Loadout:** customize and save one of five numbered presets.
- **01–05:** select a custom loadout preset; edits to the previous slot are
  saved when switching presets.
- **AR / SMG / SHOTGUN / PISTOL:** choose a weapon category, then click the
  weapon card to equip that platform in the selected primary/secondary slot.
  The catalog currently has one platform per category.
- **Save Loadout:** save the selected profile and use it for your next match.
- **During gameplay:** press **L** to open the field armory, or press
  **Esc > Edit Loadout**. Select a preset or change either weapon and click
  **Equip Loadout** to equip immediately. **Back** discards unsaved edits to
  the currently displayed preset and returns to the HUD.
- **1 / 2:** switch between equipped weapons; **R:** reload.

The five loadouts are stored through `Preferences` under
`breakbulk_loadout_<slot>_primary`, `breakbulk_loadout_<slot>_secondary`
and `breakbulk_active_loadout`. Existing `breakbulk_primary` and
`breakbulk_secondary` values are read for preset 1 as a migration fallback.
When switching profiles, the previous profile's edits are saved.

The server still validates weapon IDs against the four available definitions,
and an in-game loadout change resends the selected pair to the host.
This is a prototype gameplay loadout system; competitive restrictions such
as only allowing changes at respawn can be added separately.

## Editing viewmodels and muzzle anchors

The four GunPack models are prefab children in:

- `Assets/Prefabs/RifleViewmodel.prefab` — AssaultRifle_1.obj
- `Assets/Prefabs/SMGViewmodel.prefab` — Bullpup_1.obj
- `Assets/Prefabs/ShotgunViewmodel.prefab` — Shotgun_1.obj
- `Assets/Prefabs/PistolViewmodel.prefab` — Pistol_1.obj

Each prefab has a root viewmodel, a model child and a `MuzzleSocket` child
under the model. The GunPack meshes are authored with the barrel pointing
along **local +X**. The model child rotates 90 degrees around Y to point the
barrel forward in-game. Consequently, the muzzle socket's local X (not -Z)
must reach the barrel tip. Its transform is editable in the prefab.

The runtime now respects the **root prefab position** as a camera-relative
offset: X = right/left, Y = up/down, Z = forward/back. Use the child mesh
transform to adjust the model relative to that root; its scale and rotation
are preserved. The weapon script sets only the root's camera-follow position
and pitch/yaw (never roll). Gun effects use `Scene.GetWorldPosition` on
`MuzzleSocket`, not a 2D HUD flash.

To fine-tune the viewmodel:
1. Open the corresponding prefab in the editor.
2. Adjust the root position for overall placement and the mesh child for
   rotation/scale.
3. Position the `MuzzleSocket` child at the actual barrel tip in model-local
   coordinates.
4. Save the prefab and re-enter Play mode to respawn the weapon.

The per-weapon hip/ADS distances in `Assets/Scripts/Weapons/*.lua` are
additional offsets layered on the prefab's root position.

## Grouped UI and scroll boxes

The two armory assets (`Assets/UI/Loadout.ui` and
`Assets/UI/LoadoutInGame.ui`) are organized into real UI parent panels:
Navigation, Equipment Slots, Custom Loadouts, Weapon Classes, Weapon Catalog,
Weapon Preview, Weapon Statistics and Actions. Each catalog category has
its own `WeaponCategory_<id>` parent so the Lua controller can show/hide it
as a unit.

To add a **Scroll Box** to any UI asset:

1. Open the UI editor and choose **Palette > Panel > Scroll Box** (or
   right-click the designer and choose Scroll Box).
2. Drag buttons, panels or text widgets into it in the **Hierarchy**.
   Children use positions relative to the scroll box.
3. Set the viewport **Size** and, if needed, **Scrolling > Content Height**
   in the Details panel. You can also edit **Scroll Offset** to preview.
4. Save the UI asset. It is serialized as `ENGINE_UI 11`; older UI
   assets remain readable.
5. At runtime, mouse-wheel scrolling moves content inside the clipped
   viewport; off-screen buttons cannot be clicked.

The engine clips nested scroll-box rendering and input and retains scroll
settings when duplicating a widget. This first version supports vertical
wheel scrolling (not drag-to-scroll or horizontal scrollbars).
