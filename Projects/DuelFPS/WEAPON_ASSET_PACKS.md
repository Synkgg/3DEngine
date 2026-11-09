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

## Installed GunPack models

The four first-person prefabs now use the OBJ/MTL files committed under
`Assets/Models/GunPack/`. No external download or conversion is needed:

| Class | Installed model | Mesh yaw | Mesh scale | Muzzle socket in mesh-local coordinates |
| --- | --- | --- | --- | --- |
| Pistol | `Pistol_1.obj` | +90° | 0.47 | (1.479757, 0.563, 0) |
| Assault rifle | `AssaultRifle_1.obj` | +90° | 0.60 | (2.916603, 0.540, 0) |
| Shotgun | `Shotgun_1.obj` | +90° | 0.38 | (3.800570, 0.150, 0) |
| SMG slot | `Bullpup_1.obj` | +90° | 0.32 | (2.719206, 0.814, 0) |

**The pack has no explicitly named SMG.** `Bullpup_1` is a provisional
stand-in; it can be swapped for a preferred compact model without changing
the gameplay class or loadout save data.

The supplied OBJ files are authored with the barrel pointing along **+X**.
Each prefab rotates its mesh child by +90° around Y to point toward the
engine's **-Z** first-person forward direction. The `MuzzleSocket` is now
a child of that mesh (not the viewmodel root), positioned at the measured
front barrel area in source-model coordinates. This means scaling/rotating
the gun also moves its muzzle attachment. The Lua `Scene.FindChild` API
searches descendants so nested sockets are discovered by `WeaponSystem`.
Muzzle flash, tracer and impact effects are 3D scene prefabs, not HUD images.

These initial scales and muzzle locations were derived from OBJ geometry,
**not verified against a live rendered gameplay frame**. Inspect each
weapon in Play mode and fine-tune its grip, ADS and muzzle placement if
needed. A barrel can end before the mesh's maximum X when a sight or
accessory extends beyond it.

### Importing another weapon model

1. Confirm the asset's license allows use and distribution in games.
2. Place its model and associated MTL/textures under `Assets/Models/`.
   The loader supports OBJ+MTL or glTF/GLB, but not direct FBX import.
3. Change the `Mesh` path on entity 2 in the relevant viewmodel prefab.
4. Rotate/scale the mesh to face -Z and adjust the mesh-local
   `MuzzleSocket` child (entity 3) to the barrel exit.
5. Test hip fire, ADS, recoil and world-space muzzle VFX.
6. Save and re-export the game to include the changed asset.

Check any attribution/distribution requirements before publishing this pack.

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
