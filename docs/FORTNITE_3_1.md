# Fortnite 3.1 (Chapter 1 Season 3)

Profile: [`profiles/fortnite/3.1.toml`](../profiles/fortnite/3.1.toml).

| | |
| --- | --- |
| Build | `++Fortnite+Release-3.1-CL-3917250`, engine `4.20.0-3917250` |
| Executable SHA-256 | `648902b797d48bc6aafbb2c6ca45cfea11ea98c0b516165e86c88851eff92a18` |
| Map | `Athena_Terrain` (Erbium picks it for pre-Season 11 builds) |
| Anti-cheat | BattlEye (`_BE.exe`); no launcher/EAC presence processes needed |
| libcurl | `bVerifyPeer = false` in this build |

## Status

Verified 2026-10-21 on Fedora 44 (KDE Wayland, NVIDIA) with Proton Experimental
11.0-20261014, through `scripts/fortnite-match.sh` and the launcher's **Launch**:

- `fol-match-router.dll` with the 3.1 layouts sends account, Fortnite, content,
  lightswitch and friends requests to Lawin; login completes (`Login: Completing
  Sign-in... (Completed)`) and the Season 3 lobby loads with the starter locker.
- The game server signs in, loads Erbium (partial upstream 3.1 support, map
  `Athena_Terrain`), and listens on 7777 about 20 s after start.
- Ready → ticket → local matchmaker → join → warmup → battle bus → jump →
  storm phases (`Set AthenaGamePhase to [SafeZones]`). The end of a match has
  not been observed yet.
- The launcher shows each phase: 3.1 logs `Match State Changed from X to Y`
  where 12.50 logs `MatchState changed previous=X current=Y`.

In the game:

- After sign-in 3.1 asks Battle Royale or Save the World; pick Battle Royale.
- Pick Solo with **Change Mode** once; the choice is saved with your settings.
  Erbium ignores playlists before 3.5, so the server's game mode is the build's
  default either way.

Not working / not checked:

- Global chat ("Unable to connect to the chat server"): the Lawin XMPP adapter
  has no chat rooms (same as 12.50). Party messages work once XMPP connects.
- Only one player has been tested.

## Setup notes

- The Erbium toolchain (msvc-wine) needs `msiextract`: `sudo dnf install msitools`
  (Fedora) or `pacman -S msitools`. Without root, extract the Fedora `msitools`,
  `libmsi1` and `libgcab1` RPMs locally and put them on `PATH`/`LD_LIBRARY_PATH`.
- msvc-wine's default is VS 18; this build used `--major 17` (MSVC 14.44.35207,
  SDK 10.0.26100.0).

## Encrypted executable

The main `.text` section (46 MB) has entropy 7.99 on disk: it is encrypted and
decrypted at startup by a stub in a second `.text` section. `.rdata` (vtables,
strings) is plain. The router layouts therefore came from a memory image of the
running game, read from `/proc/<pid>/mem` after startup (entropy 6.54), not from
the file.

## HTTP layouts

`FCurlHttpRequest`'s vtable follows 4.20's `IHttpBase`/`IHttpRequest` declaration
order, with `~IHttpBase` declared last in `IHttpBase` (slot 7). This gives the
same slots as 12.50 for `SetURL` (10) and two fewer for `ProcessRequest` (15, not
17).

| | RVA | Notes |
| --- | --- | --- |
| vtable | `0x36d13c0` | Request objects logged by `LogHttp` (`%p: libcurl …`) point here |
| `ProcessRequest` (slot 15) | `0x1adbda0` | |
| `SetURL` (slot 10) | `0x1adf020` | `add rcx, 0x28; jmp FString::operator=` |
| URL `FString` | `this + 0x28` (40) | `GetURL` (slot 0) copies from `+0x28` |

`http_observer_layout = "28163488,57480128,40,15"`,
`http_router_layout = "28176416,10"`.

A second, similar vtable at `0x36d1808` holds the function with the
`ProcessRequest failed. URL '%s' is not a valid HTTP request` string and keeps
its URL at `+0x18`, but the requests the game sends don't use it: hooking it
installed cleanly and observed nothing.
