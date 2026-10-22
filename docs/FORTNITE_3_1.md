# Fortnite 3.1 (Chapter 1 Season 3)

Port in progress. Profile: [`profiles/fortnite/3.1.toml`](../profiles/fortnite/3.1.toml).

| | |
| --- | --- |
| Build | `++Fortnite+Release-3.1-CL-3917250`, engine `4.20.0-3917250` |
| Executable SHA-256 | `648902b797d48bc6aafbb2c6ca45cfea11ea98c0b516165e86c88851eff92a18` |
| Map | `Athena_Terrain` (Erbium picks it for pre-Season 11 builds) |
| Anti-cheat | BattlEye (`_BE.exe`); no launcher/EAC presence processes needed |
| libcurl | `bVerifyPeer = false` in this build |

## Status

- **Verified (2026-10-21, Fedora, Proton Experimental 11.0-20261014):** startup to
  the frontend in the loopback-only namespace; `fol-match-router.dll` with the 3.1
  layouts rewrites account (`/account/api/oauth/token`) and version-check requests
  to `http://127.0.0.1:3551`.
- **Not yet:** Lawin login, lobby, the game server's Erbium login marker
  (`server_login_marker`, so the match script refuses to host for 3.1), gameplay.
  3.1 logs no `UI State changing` lines, so the 12.50 marker does not apply.

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
