# Playing a local 12.50 BR Solo match

`scripts/fortnite-match.sh` runs everything needed for a private match inside
one loopback-only network namespace:

1. **Lawin backend** (login, profiles, party, XMPP) plus local **matchmaking**:
   ticket → matchmaker websocket on `127.0.0.1:3552` → session pointing at
   `127.0.0.1:7777`. The matchmaker keeps the client *Queued* until the game
   server's UDP port is actually bound, then sends *Play*.
2. A **game server**: a second 12.50 process signed in with the same local
   account. `fol-inject.exe` loads `fol-match-router.dll` (persistent local
   backend redirect) shortly after start, waits for the sign-in-complete log
   line, then loads the **Erbium** game server built with
   `integrations/windows/erbium_solo_config.h` (Playlist_DefaultSolo, no cheats,
   no siphon, no GUI). Erbium opens `Apollo_Terrain` and listens on 7777.
3. The **client**, with the same router, rendered through Proton/DXVK.

## One-time build

```sh
./build.sh                       # native CLI/backend runner
bash helpers/windows/build.sh    # fol-inject.exe, fol-match-router.dll, input helper
unshare --user --map-root-user --net -- python3 integrations/windows/build_module.py \
  --provider erbium --reference /path/to/Erbium --toolchain /path/to/msvc-toolchain \
  --config integrations/windows/erbium_solo_config.h --jobs 8
```

The Lawin deployment, Proton runtime and prefix are the ones from
[BACKEND.md](BACKEND.md) / [integrations/lawin](../integrations/lawin/README.md).

## Play

```sh
scripts/fortnite-match.sh
```

Wait for `Game server listening on 127.0.0.1:7777`, then the client starts.
Select Battle Royale → Solo → Play. Matchmaking connects you to the local
server. Logs for each run are in `~/.local/state/fortnite-on-linux/logs/sessions/<date>_match/`
(`server-game.log`, `client-game.log`, `lawin.log`, wine logs).

Other modes: `--server` (game server only). `FOL_MATCH_AUTOPLAY=1` is a test
mode that fullscreens the client, presses Play with the Win32 input helper and
saves screenshots into the session directory.

Overrides: `FOL_GAME_DIR`, `FOL_PROTON`, `FOL_ERBIUM_DLL`.

## Notes

- **Server window:** this Shipping build ignores `-nullrhi`, so the server runs
  with `-RenderOffScreen` at 640x360 and `fol-server-guard.dll` keeps its
  windows hidden, disabled and uncloseable. A close request used to destroy its
  viewport and Erbium crashed on a null world (`TickFlush__RepGraph`).
- **"Fortnite was not started correctly":** 12.50's environment check
  (`LogAppES: closing code 1`) closes the game after a while unless the
  launcher context exists. `fol-inject` starts `FortniteLauncher.exe` and the
  `_EAC` bootstrap next to each instance with `CREATE_SUSPENDED`; they never
  run and die with the game.
- **Hyprland fullscreen:** with per-region damage tracking, a fullscreen
  XWayland game only repainted where the cursor moved. The script switches
  `debug:damage_tracking` to 1 (monitor) during a session and restores it, and
  makes the focused monitor the X11 primary so its resolution (e.g. 1440p) is
  offered. NVIDIA: `__NV_DISABLE_EXPLICIT_SYNC=1`.
- Erbium's ImGui window crashed the server's D3D11 render thread under DXVK,
  so the Solo config disables it (bus starts on the normal warmup timer) and
  its console window is hidden.
- Client and server share one local account. Nothing leaves the namespace.

## Verified result (2026-10-01)

Session `2026-10-01_03-50-14_match`, Proton Experimental 20260924, CachyOS/Hyprland/RTX 4070 SUPER:

- Server: Erbium loaded ~5 s after sign-in, `Apollo_Terrain` brought up,
  `IpNetDriver listening on port 7777`, MatchState `WaitingToStart`.
- Client: Play → ticket 200 → matchmaker Play → session 200 → join 204 →
  `Welcomed by server (Level: Apollo_Terrain, Game: Athena_GameMode)`.
- Server: `PostLogin`, `Join succeeded: Local Player`, MatchState `InProgress`.
- Client spawned on the Chapter 2 Season 2 map with full HUD (100 HP, pickaxe,
  build bar, minimap); a pickaxe swing replicated.

Earlier failure fixed on the way: Lawin revoked the account's existing tokens
on every login, so the client's sign-in logged the server out ("Logged In
Elsewhere") and it left the map. `integrations/lawin/multi-session.cjs` keeps
earlier sessions of the local account valid.

## Launcher

`build/project-tux` (Qt) wraps `scripts/fortnite-match.sh`: it runs the script in
its own process group so **Stop** signals everything at once (Lawin's runner
stops MongoDB/backend; the match script's cleanup kills the Wine processes).
The **Hosting** checkbox picks `--both` (default) or `--client`. The **Lawin**
tab drives `scripts/lawin-admin.sh` (`integrations/lawin/admin.cjs`), which
starts a private MongoDB in its own loopback-only namespace, applies one change,
backs up the profile under the deployment's `profile-backups/`, and stops. It
refuses to run while a game session holds the database lock.

Verified 2026-10-01: Launch from the GUI → server listening → client joined →
battle bus; Stop left no processes or lock. Name/V-Bucks edits showed in the
lobby (hosting off).
