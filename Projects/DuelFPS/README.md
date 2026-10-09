# BREAKBULK

An original, compact 1v1 cargo-terminal shooter built in Velcryn. The existing
DuelFPS project descriptor now launches BREAKBULK.

## Play

From the repository root:

```powershell
.\out\build\x64-Release\VelcrynEditor.exe .\Projects\DuelFPS\DuelFPS.project
```

Press **Play** in the editor. Choose **Host a Duel** on one instance.
On another PC on the same LAN, choose **Find LAN Servers**: the new server
browser scans UDP port 7777 without blocking the game, lists discovered session
names, IPv4 addresses and player counts, and lets you join with one click.
Select **Refresh** to scan again; full sessions cannot be joined. You can also
use the existing direct-IP **Connect** field (or `127.0.0.1` on the same PC).

**LAN discovery is not an internet-wide public server directory.** It uses UDP
broadcasts, so both computers must be on a reachable local network and Windows
Firewall must allow the game/UDP 7777. For players on different public IPs,
continue using direct IP and configure the host's port forwarding/firewall.
A worldwide server browser requires a hosted master-server directory and
public-server registration (plus NAT traversal or port forwarding). No such
internet service is included. **Practice Range** runs the target drill offline.

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
