# Emberwild

A playable solo/two-player survival expedition for Velcryn. Original procedural
forest, campsite, spring, gathering nodes and survivor models; no downloaded art.

## Play now

From the repository root, run `powershell -File Tools/Play-Emberwild.ps1`.
Or open `Projects/Emberwild/Emberwild.project` in the editor and press Play.
The standalone export is in `out/Emberwild`; run `Game.exe` there.

Choose **Play Solo**, or **Host Camp** on one computer and **Join IP** on the other
for LAN play. Up to two people share supplies, a campfire and the rescue objective.

## Expedition

Each day lasts three minutes; night starts 110 seconds into each day. Explore
within the cliffs, gather wood/stone/berries, and drink at the blue spring east
of camp. Resources regrow after 55 seconds. Keep the fire fed to heal near camp
and stay warm at night. Craft an axe to gather faster and a coat to reduce cold
damage. Build the rescue signal at the mast, then survive nine minutes with the
fire at least 20% full to win. A downed player can be revived by their partner.
If everyone is down, the expedition ends. Escape opens the menu to leave/restart.

| Control | Action |
|---|---|
| WASD / mouse | Move / look |
| Shift / Space | Sprint / jump |
| E | Gather nearby node, drink at spring, or add wood at fire |
| F | Eat one berry ration |
| Q at fire | Cook a meal: one wood + one berry ration |
| 1 | Craft axe: three wood + two stone |
| 2 | Craft coat: four wood + three berry rations |
| R at mast | Build signal: twelve wood + eight stone |
| V near partner | Revive: two berry rations |
| Tab / Escape | Field guide / camp menu |

The solo menu pauses survival; online menus leave the shared world running.
Expeditions are session-based; no save/load or host migration is included.

## Internet play without a VPN

The TLS relay and helper are in `Tools/emberwild/relay.py`. Both players make
outbound TCP connections to a public relay. No player router port forwarding or
VPN is needed. **A public relay has not been deployed or configured for you.**
See `Tools/emberwild/README.md` for setup. LAN alone cannot cross home NATs.

Once the relay is running:

1. Host runs the helper with role `host`, then chooses **Host Camp**.
2. Guest runs the helper with the same room code and role `guest`, then chooses
   **Join via Relay**. Keep both helpers running during the game.
3. If a helper disconnects, restart it and return to the main menu to rejoin.

## Development and verification

- `python Tools/build_emberwild.py` regenerates the assets and layout. Gameplay
  scripts are hand-authored and are not overwritten.
- `VelcrynEditor.exe --network-smoke-test` runs the pure Lua simulation tests as
  well as existing socket and shooter regressions (run from the repository root).
- `VelcrynEditor.exe --rhi-smoke-test` tests actual host/join menu flows, both Lua
  runtimes, replicated gathering with duplicate suppression, and GPU captures.
- `python Tools/emberwild/test_relay.py` checks TLS, room isolation, occupied-role
  rejection, teardown, and both shipping UDP bridges. Only this test requires
  the Python `cryptography` package; the relay itself is standard-library-only.

Host validates interaction distance, resource availability, recipes and action
sequences. Clients receive full sequenced snapshots every 100ms, so packet loss
and late joining recover without replaying world history. Movement is still
client-reported; this is a private cooperative game, not an anti-cheat system.
TLS/TCP relay transport can stall under packet loss; LAN/direct UDP is lower
latency. Internet latency and two separate home networks still need playtesting.

