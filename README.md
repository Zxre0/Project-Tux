# Project Tux

Play old Fortnite builds on Linux: a native launcher/CLI, a local backend and a
local game server, running the Windows game through Proton/DXVK.

**Status:** local BR Solo works end to end. Log in, customise your locker, press
Play, get matched to the local game server, ride the bus and drop onto the map.

| Version | Season | Tested on | Notes |
| --- | --- | --- | --- |
| **12.50** | Chapter 2 Season 2 | CachyOS, Hyprland, NVIDIA | Original target; login, lobby and match join verified ([notes](docs/MATCH.md#verified-result-2026-10-01)) |
| **3.1** | Chapter 1 Season 3 | Fedora 44, KDE Wayland, NVIDIA | Login, lobby, bus and storm verified ([notes](docs/FORTNITE_3_1.md)) |

The launcher reads the version from your game files and picks the matching
profile in [`profiles/fortnite/`](profiles/fortnite). Both Arch/CachyOS and
Fedora are supported (see [Fedora](docs/LINUX.md#fedora)).

> You need your own legitimately obtained game files for a supported version.
> No Fortnite assets, game-server binaries or Epic credentials are distributed
> here. Everything runs in an isolated, loopback-only network namespace;
> nothing talks to Epic.

## How it works

```
 ┌──────────── isolated network namespace (loopback only) ─────────────┐
 │                                                                     │
 │  Lawin backend :3551   login, profiles, locker, party, XMPP         │
 │  Matchmaker    :3552   queues you until the game server is ready    │
 │                                                                     │
 │  Game server  (Fortnite + Erbium)  ──  :7777                        │
 │        ▲          12.50: Apollo_Terrain · 3.1: Athena_Terrain       │
 │        │ join                                                       │
 │  Client       (Fortnite, Proton/DXVK, your window)                  │
 └─────────────────────────────────────────────────────────────────────┘
```

- **Backend:** [LawinServerV2](https://github.com/Lawin0129/LawinServerV2), run
  through the adapter in [`integrations/lawin`](integrations/lawin/README.md),
  with local matchmaking added.
- **Game server:** [Erbium](https://github.com/plooshi/Erbium), built on Linux
  with the MSVC toolchain and a plain-Solo config (no cheats, no siphon).
- **Glue:** `fol-inject.exe` starts each game process and loads
  `fol-match-router.dll` (redirects Epic service URLs to the local backend)
  and, for the server, Erbium once sign-in completes. The version's profile
  supplies the router's hook locations, launch arguments and the sign-in line
  to wait for; the router only runs on the exact executable it was made for.

## Quick start

1. **Build.** Install the build dependencies first:

   ```sh
   # Arch/CachyOS
   sudo pacman -S --needed base-devel cmake ninja qt6-base tomlplusplus zlib \
       vulkan-headers vulkan-icd-loader clang lld wine nodejs passt
   # Fedora
   sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel tomlplusplus-devel \
       zlib-ng-compat-devel vulkan-headers vulkan-loader-devel clang lld \
       wine wine-devel nodejs passt
   ```

   Then:

   ```sh
   ./build.sh                      # native CLI + backend runner + tests
   bash helpers/windows/build.sh   # Windows helpers (needs Wine headers/libs)
   ```

2. **One-time setup:** register your game install (`fortnite-linux add
   /path/to/Fortnite`), import Proton, and prepare the Lawin deployment. See
   [docs/BACKEND.md](docs/BACKEND.md) and
   [integrations/lawin/README.md](integrations/lawin/README.md).

3. **Build the game server** (Erbium) with the Solo config. The same DLL serves
   every supported version. It needs the MSVC toolchain from
   [msvc-wine](https://github.com/mstorsjo/msvc-wine), which needs `msitools`
   (`pacman -S msitools` / `dnf install msitools`). See
   [docs/MATCH.md](docs/MATCH.md#one-time-build).

4. **Play** with the launcher:

   ```sh
   build/project-tux                 # or: scripts/install-desktop.sh, then "Project Tux" in your app menu
   ```

   Press **Launch**. It starts the backend, the game server and your game, and
   shows each one's status. In the lobby choose **Battle Royale → Solo → Play**
   (on 3.1: pick **Battle Royale** after sign-in, set **Solo** with **Change
   Mode** once, then **Ready**). **Stop** (or closing the game) ends everything.
   Prefer a terminal? `scripts/fortnite-match.sh` does the same thing.

## Updating

In the launcher, open **Settings → Updates**. It checks GitHub when it starts
(the Play tab shows *Update available* when there is one). Press **Update now**,
then restart when it asks. From a terminal:

```sh
./update.sh            # download the latest main and rebuild
./update.sh --check    # only show what's new
```

It updates `main` (switch with `git switch main` if you're on another branch),
rebuilds the launcher, CLI and Windows helpers, and leaves your settings, game
files and Lawin locker alone. If you changed any of the project's files it stops
and lists them. It tells you when the Erbium game server or the Lawin setup also
needs redoing.

## Launcher

| Tab | What it does |
| --- | --- |
| **Play** | One-click Launch/Stop, live status of backend, game server and game, match phase, session log |
| **Hosting** | *Host a local game server* (on by default). Turn it off to start only the backend and your game (lobby/locker work; Play queues with no server). Erbium DLL path. |
| **Multiplayer** | Off by default. Join another player's server by IP/hostname, or host so LAN/VPN players can join yours (needs `passt`). When joining a [Project Tux Windows Server](https://github.com/Zxre0/Project-Tux-Windows-Server), use **the server's accounts** so your cosmetics show in its matches ([details](docs/MATCH.md#shared-accounts-joining-a-project-tux-windows-server)). The sandbox still can't reach Epic. |
| **Lawin** | Edit the local backend: display name, level, Battle Pass tier, V-Bucks, add/remove cosmetics (all 6,000+ Lawin knows; the game skips items newer than your build), unlock everything, reset locker, edit the item shop. Available while the game isn't running. |
| **Settings** | Game folder (its version is detected), Proton path, a setup checklist that tells you what's missing, Discord status, and updates |

`project-tux --launch` starts straight away; the desktop entry has a *Play now*
action for that. The Lawin editor is also a CLI: `scripts/lawin-admin.sh info`.

**Discord:** while a session runs, the launcher shows *Playing Project Tux
Fortnite on Linux* in Discord, with the version and match phase underneath.
Project Tux's Discord application (ID `1562653073317568513`) is built in; to use
your own, create one at
[discord.com/developers/applications](https://discord.com/developers/applications)
and put its **Application ID** in Settings → Discord. It talks only to the
Discord app on your computer (native, Flatpak or Vesktop), never from inside
the game's sandbox; untick the box to turn it off.

The server shows a small frozen Fortnite window and an Erbium console. Leave
them open. Logs for every run go to
`~/.local/state/fortnite-on-linux/logs/sessions/<date>_match/`.

## CLI

The `fortnite-linux` binary manages installs, Proton runtimes and prefixes, and
runs diagnostics:

```sh
fortnite-linux discover /mnt/games        # find installs
fortnite-linux add /path/to/Fortnite      # register one (ID = detected version, e.g. 3.1)
fortnite-linux verify 3.1                 # structural/manifest checks
fortnite-linux runtime import-proton '/path/to/Proton - Experimental' --id experimental
fortnite-linux prefix create 3.1
fortnite-linux prefix backup 3.1
fortnite-linux diagnose
fortnite-linux logs analyze /path/to/fortnite.log
fortnite-linux --help
```

Full command notes and caveats: [docs/CLI.md](docs/CLI.md).

## Known limitations

- Only 12.50 and 3.1 BR have been tested, with a single player.
- Other versions need their own profile: the router's hook locations are tied
  to one executable. [FORTNITE_3_1.md](docs/FORTNITE_3_1.md) shows how they were
  found for 3.1, whose executable is encrypted on disk.
- Global chat doesn't connect ("Unable to connect to the chat server"): the Lawin
  XMPP adapter has no chat rooms.
- 12.50's Shipping build ignores `-nullrhi`, so the server still renders a window.
- Erbium's ImGui control panel crashes the server under DXVK and is disabled,
  so the bus launches on the normal warmup timer.
- The Erbium DLL isn't committed; build it yourself (step 3).

## Documentation

| Doc | Contents |
| --- | --- |
| [MATCH.md](docs/MATCH.md) | Running a local match, how it works, verified result (12.50) |
| [BACKEND.md](docs/BACKEND.md) | Lawin setup and the lobby launch |
| [CLI.md](docs/CLI.md) | Detailed CLI behaviour |
| [LINUX.md](docs/LINUX.md) | Host requirements and notes |
| [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) | Common problems |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Code layout |
| [FORTNITE_12_50.md](docs/FORTNITE_12_50.md) | 12.50 build notes and startup history |
| [FORTNITE_3_1.md](docs/FORTNITE_3_1.md) | 3.1 notes: encrypted executable, HTTP layouts, setup, status |
| [PROGRESS.md](docs/PROGRESS.md) | Full development log |

## Credits

[LawinServerV2](https://github.com/Lawin0129/LawinServerV2) (GPL-3.0) and
[Erbium](https://github.com/plooshi/Erbium) (GPL-3.0) are used from pinned
upstream checkouts and are not vendored here.
[msvc-wine](https://github.com/mstorsjo/msvc-wine) provides the Linux-hosted
MSVC toolchain. Fortnite is a trademark of Epic Games; this project is not
affiliated with or endorsed by Epic Games.
