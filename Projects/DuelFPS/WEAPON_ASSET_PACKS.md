# Weapon model packs and muzzle sockets

BREAKBULK's four loadout weapons are currently **Mako P12** (pistol),
**Kestrel AR4** (assault rifle), **Breach S8** (shotgun) and
**Vector K9** (SMG). Each uses a separate viewmodel prefab:

| Weapon | Prefab |
| --- | --- |
| Pistol | `Assets/Prefabs/PistolViewmodel.prefab` |
| Assault rifle | `Assets/Prefabs/RifleViewmodel.prefab` |
| Shotgun | `Assets/Prefabs/ShotgunViewmodel.prefab` |
| SMG | `Assets/Prefabs/SMGViewmodel.prefab` |

## Using a purchased or free weapon pack

1. Confirm that the asset license permits use and distribution in games.
2. Copy the model and its textures into a folder under
   `Projects/DuelFPS/Assets/Models/` (for example, `Models/ImportedWeapons`).
3. Use **OBJ + MTL + textures** or **glTF/GLB** models. The current model
   loader does **not** support direct FBX import. Convert FBX to glTF/GLB or
   OBJ first, keeping the original license and texture maps.
4. In each weapon prefab, replace the gun mesh path on the model child,
   keeping the prefab root and its separate `MuzzleSocket` child.
5. In the prefab editor, position and orient the model so the barrel points
   along **local -Z**. Adjust model scale/rotation without changing the
   first-person viewmodel root. Move `MuzzleSocket` to the actual barrel exit.
6. Save the prefab and test both hip fire and ADS. The 3D muzzle flash,
   tracer and impact system reads the socket's transformed world position
   when the weapon fires.

The muzzle socket is a real **transform entity**, not a UI element and not
a baked-in fixed distance. The current default sockets are:

| Weapon | MuzzleSocket local position (x, y, z) |
| --- | --- |
| Mako P12 | (0, 0.015, -0.585) |
| Kestrel AR4 | (0, 0.005, -1.170) |
| Breach S8 | (0, 0.030, -1.230) |
| Vector K9 | (0, 0.005, -0.870) |

A prefab missing the socket still works using the legacy camera-relative
fallback, but it will be less accurate. Keeping the socket makes the VFX
follow recoil, ADS and any new model's barrel geometry.

**Do not copy the model's license into the game unless required or permitted.**
If the pack requires attribution, include its required attribution in the
distributed game. Assets must be present under the project asset directory
before using **File > Export Game**; the exporter packages saved project assets.

## Replacing the armory artwork

The loadout screen is authored in `Assets/UI/Loadout.ui`. Its current weapon
showcase uses lightweight blueprint-style silhouettes. Once weapon pack assets
are supplied, they can be replaced with rendered thumbnails or a dedicated
3D model-preview implementation. Merely changing a prefab's mesh path does not
automatically generate a UI thumbnail.

If you upload an asset pack, supply the archive and license plus which model
should map to each of the four weapon classes. The viewmodel meshes, material
maps, muzzle socket placements and loadout artwork can then be updated to
match the pack.
