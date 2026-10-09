# BREAKBULK

An original, compact 1v1 cargo-terminal shooter built in Velcryn. The existing
DuelFPS project descriptor now launches BREAKBULK.

## Play

From the repository root:

```powershell
.\out\build\x64-Release\VelcrynEditor.exe .\Projects\DuelFPS\DuelFPS.project
```

Press **Play** in the editor. Choose **Create Duel** on one instance and
**Connect** on the other. On the same PC, use `127.0.0.1`. On a LAN, enter the
host's IPv4 address. The game uses UDP port 7777. Internet direct-IP sessions
require that port to reach the host; there is no matchmaking service or automatic
NAT traversal. **Explore Terminal** runs the map and target drill offline.

| Control | Action |
|---|---|
| WASD / mouse | Move / look |
| Left Shift / Space | Sprint / jump |
| Left mouse / right mouse | Fire / aim |
| R / 1 / 2 | Reload / Mako sidearm / Kestrel carbine |
| Escape | Session menu; online simulation continues |

## Match rules

- Exactly two players; a third connection is rejected.
- First to five rounds. Three-second deployment countdown and round break.
- Equal sidearm/carbine loadouts; both weapons replenish every round.
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

The dispatch menu, map diagram, HUD, countdown, death/round/match results and
pause screen are newly authored. The game also includes original procedural
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

- `Game.Network`: real loopback sockets, full-lobby rejection, reconnection,
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
