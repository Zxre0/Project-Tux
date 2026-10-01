# Project Tux

Play **Fortnite 12.50** (Chapter 2 Season 2) on Linux: a native launcher/CLI,
a local backend and a local game server, running the Windows game through
Proton/DXVK.

**Status:** local BR Solo works end to end on CachyOS/Hyprland/NVIDIA with Proton
Experimental. Log in, customise your locker, press Play, get matched to the
local game server, ride the bus and drop onto the map.

> You need your own legitimately obtained 12.50 game files. No Fortnite assets,
> game-server binaries or Epic credentials are distributed here. Everything
> runs in an isolated, loopback-only network namespace; nothing talks to Epic.

## How it works

```
 ┌──────────── isolated network namespace (loopback only) ─────────────┐
 │                                                                     │
 │  Lawin backend :3551   login, profiles, locker, party, XMPP         │
 │  Matchmaker    :3552   queues you until the game server is ready    │
 │                                                                     │
 │  Game server  (Fortnite 12.50 + Erbium)  ──  Apollo_Terrain :7777   │
 │        ▲                                                            │
 │        │ join                                                       │
 │  Client       (Fortnite 12.50, Proton/DXVK, your window)            │
 └─────────────────────────────────────────────────────────────────────┘
```

- **Backend:** [LawinServerV2](https://github.com/Lawin0129/LawinServerV2), run
  through the adapter in [`integrations/lawin`](integrations/lawin/README.md),
  with local matchmaking added.
- **Game server:** [Erbium](https://github.com/plooshi/Erbium), built on Linux
  with the MSVC toolchain and a plain-Solo config (no cheats, no siphon).
- **Glue:** `fol-inject.exe` starts each game process and loads
  `fol-match-router.dll` (redirects Epic service URLs to the local backend)
  and, for the server, Erbium once sign-in completes.

## Quick start

1. **Build** (Arch/CachyOS packages: `base-devel cmake ninja qt6-base
   tomlplusplus zlib vulkan-headers vulkan-icd-loader clang lld`):

   ```sh
   ./build.sh                      # native CLI + backend runner + tests
   bash helpers/windows/build.sh   # Windows helpers (needs Wine headers/libs)
   ```

2. **One-time setup:** register your 12.50 install, import Proton, and prepare
   the Lawin deployment. See [docs/BACKEND.md](docs/BACKEND.md) and
   [integrations/lawin/README.md](integrations/lawin/README.md).

3. **Build the game server** (Erbium) with the Solo config. See
   [docs/MATCH.md](docs/MATCH.md#one-time-build).

4. **Play:**

   ```sh
   scripts/fortnite-match.sh
   ```

   Wait for `Game server listening on 127.0.0.1:7777`. The client then opens:
   choose **Battle Royale → Solo → Play**.

The server shows a small frozen Fortnite window and an Erbium console. Leave
them open. Logs for every run go to
`~/.local/state/fortnite-on-linux/logs/sessions/<date>_match/`.

## CLI

The `fortnite-linux` binary manages installs, Proton runtimes and prefixes, and
runs diagnostics:

```sh
fortnite-linux discover /mnt/games        # find installs
fortnite-linux add /path/to/Fortnite      # register one
fortnite-linux verify 12.50               # structural/manifest checks
fortnite-linux runtime import-proton '/path/to/Proton - Experimental' --id experimental
fortnite-linux prefix create 12.50
fortnite-linux prefix backup 12.50
fortnite-linux diagnose
fortnite-linux logs analyze /path/to/fortnite.log
fortnite-linux --help
```

Full command notes and caveats: [docs/CLI.md](docs/CLI.md).

## Known limitations

- Only 12.50 and BR Solo have been tested, with a single player.
- This Shipping build ignores `-nullrhi`, so the server still renders a window.
- Erbium's ImGui control panel crashes the server under DXVK and is disabled,
  so the bus launches on the normal warmup timer.
- The Erbium DLL isn't committed; build it yourself (step 3).

## Documentation

| Doc | Contents |
| --- | --- |
| [MATCH.md](docs/MATCH.md) | Running a local match, how it works, verified result |
| [BACKEND.md](docs/BACKEND.md) | Lawin setup and the lobby launch |
| [CLI.md](docs/CLI.md) | Detailed CLI behaviour |
| [LINUX.md](docs/LINUX.md) | Host requirements and notes |
| [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) | Common problems |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Code layout |
| [FORTNITE_12_50.md](docs/FORTNITE_12_50.md) | 12.50 build notes and startup history |
| [PROGRESS.md](docs/PROGRESS.md) | Full development log |

## Credits

[LawinServerV2](https://github.com/Lawin0129/LawinServerV2) (GPL-3.0) and
[Erbium](https://github.com/plooshi/Erbium) (GPL-3.0) are used from pinned
upstream checkouts and are not vendored here.
[msvc-wine](https://github.com/mstorsjo/msvc-wine) provides the Linux-hosted
MSVC toolchain. Fortnite is a trademark of Epic Games; this project is not
affiliated with or endorsed by Epic Games.
