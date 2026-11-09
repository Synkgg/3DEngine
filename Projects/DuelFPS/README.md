# BREAKBULK

An original, compact cargo-terminal elimination shooter built in Velcryn. The existing
DuelFPS project descriptor now launches BREAKBULK.

## Play

Choose **Mode** beside **Play** to cycle through **1v1**, **2v2**, and **3v3**. Play matches players who chose the same mode on the configured dedicated server. For a private team match, select **Custom game → DuelFPS server → Create server game**, choose a mode and share the lobby code. See the [Lightsail deployment instructions](Server/README.md) to set up the online endpoint.

Choose **Custom game** to pick Terminal or Freight Depot, rounds to win (3/5/7), round duration (60/90/120 seconds), and connection type. Select **DuelFPS server** for an online lobby or **LAN** to host a 1v1 on your local network. Joining players receive the host's rules and map. **Training Range** offers a separate map picker. **Weapons** opens the workbench: choose a custom class, click its primary or secondary card, then browse, equip and save. Cancel discards unsaved edits. Hold **Left Ctrl** to crouch; rebind it in Settings → Controls. The minimap is north-up and marks your position, not hidden enemies. Match hitmarkers indicate damage confirmed by the host.

From the repository root:

```powershell
.\out\build\x64-Release\VelcrynEditor.exe .\Projects\DuelFPS\DuelFPS.project
```

For a LAN match, both players select **Custom game**, then click **Host on** to switch from **DuelFPS server** to **LAN**. One player chooses **Create LAN game** and the other chooses **Find LAN games**. Both computers must be on the same local network with UDP 7777 allowed. For online play, leave the toggle on **DuelFPS server** and share the eight-digit lobby code. **Training range** runs offline.

| Control | Action |
|---|---|
| WASD / mouse | Move / look |
| Left Shift / Space | Sprint / jump |
| Left mouse / right mouse | Fire / aim |
| R / 1 / 2 | Reload / equipped primary / equipped secondary |
| Escape | Session menu; online simulation continues |

## Match rules

- Exactly two players; a third connection is rejected.
- First to five rounds. Three-second deployment countdown and round break.
- Four selectable weapons (pistol, assault rifle, shotgun and SMG). Each
  player saves two distinct loadout slots in the armory; both replenish
  ammunition every round.
- Ninety-second rounds. On timeout, higher health wins; equal health is a draw.
- Damage and round scores are decided by the host, with host-side raycasts,
  weapon-defined damage, origin/rate checks and duplicate-shot suppression.
- Both players request a rematch to start another match.
- Disconnects return the host to waiting; the second player slot is reusable.

## New content

Terminal is a three-route yard with six corrugated containers, two open tunnel
cuts, staggered crate cover, distinct opposing insertions, lane markings, a crane,
freight stacks and industrial lamps. Collider dimensions match the static map;
the two insertion points cannot shoot directly through the central cover.

The dispatch menu, tactical armory loadout, map diagram, HUD, countdown,
death/round/match results and pause screen are newly authored. The armory
includes two editable slots, four weapon rows, live platform details and
performance ratings. The platform display uses schematic silhouettes until
model-pack thumbnails are provided. The game also includes original procedural
Kestrel/Mako weapon models, an armored remote operator, synthesized firing/UI/
reload/hit/footstep audio, aim/recoil/reload motion and damage feedback.

`Tools/build_breakbulk.py` reproducibly authors the new OBJ/MTL models, map,
prefabs, UI and sounds using Python's standard library. Gameplay builds on the
engine's existing movement, scripting and UDP systems. The old PracticeRange is
retained as a renderer regression fixture; the new menu uses TerminalDrill.

## Validation

```powershell
ctest --test-dir out/build/x64-Release --output-on-failure
```

- `Game.Network`: real loopback sockets, LAN discovery and advertised
  player counts, full-lobby rejection, reconnection,
  transform/message delivery and host shutdown; shipping Lua scripts exercised
  through five rounds, victory, mutual rematch, draw, reload and client hit checks.
- `RHI.Smoke`: GPU map/menu/HUD readbacks, menu hover, map collision rays, plus
  **two real Runtime instances** using actual Lua bindings, physics and UDP.
  It verifies menu-to-drill switching and a fired shot producing a round win
  with matching scores on both peers.

Captures are saved under `out/build/breakbulk-*.ppm`. These tests are local;
multi-machine internet latency and long play sessions still need playtesting.
Movement remains client-reported and there is no lag compensation or production
anti-cheat. Assets are deliberately stylized procedural geometry, not a AAA art
library. Audio playback itself has not been assessed by an automated test.

## Weapon presentation and sound

Each viewmodel prefab contains an editable `MuzzleSocket` transform at the
barrel exit. Gunfire uses the transformed world position to spawn short-lived
**3D muzzle flashes, tracers and impacts**. The muzzle follows aim/recoil
rather than being drawn as a fake UI element. Older prefabs without a socket
fall back to the camera-relative offset. See [WEAPON_ASSET_PACKS.md](WEAPON_ASSET_PACKS.md)
for importing licensed OBJ/glTF/GLB weapon models and repositioning sockets.

Footsteps now raycast the actual ground surface and select distinct
metal/concrete/wood procedural boot sounds, with separate sprint/walk cadence
and randomized pitch/texture. No external footstep audio assets are required.

## Simplified UI and settings

The main menu now has three choices: **Play**, **Loadout**, and **Settings**.
Custom game offers LAN discovery and DuelFPS server lobbies. The loadout
screen shows both slots and all four weapons together. The HUD keeps health,
ammunition, score, and the clock; match feedback appears only when needed.

Open **Settings** from the main menu or the in-game Escape menu:

- **Controls:** mouse sensitivity, aim sensitivity multiplier, field of view,
  invert Y, and bindings for movement, sprint, jump, fire, aim, reload, both
  weapon slots, interaction, and loadout. Click a binding, then press a keyboard
  key or mouse button (including side buttons). Conflicts swap the two actions.
  Escape cancels capture and always remains available to close menus. Capture
  times out after ten seconds. Mouse wheel bindings are not supported.
- **Audio:** master, game sounds, and menu sounds, plus a test sound. Sliders
  affect sounds already playing as well as new sounds.
- **Graphics:** Low/Medium/High presets, shadows, anti-aliasing, bloom, fog, view
  distance, and brightness. Presets also adjust reflections and indirect light.

Changes preview immediately. **Apply & Back** saves them to
`Saved/duel_settings_v1.txt`; **Cancel** restores the previous settings.
**Reset Defaults** stays a preview until applied. Settings are shared between
lobby and gameplay and load on the next launch. Online matches keep running
while menus are open; menus block movement, fire, aiming and interaction.

`Tools/build_duelfps_ui.py` regenerates these UI assets without changing saved
preferences or gameplay. The settings pages reuse one visual asset with lobby
and gameplay callback wrappers. The engine's network test includes settings
persistence, conflict-swapping, reset/cancel and save-error tests. Its GPU test
also opens the real settings UI, captures a side mouse binding, previews audio
and graphics, applies/reopens/cancels, and returns from in-game settings.
