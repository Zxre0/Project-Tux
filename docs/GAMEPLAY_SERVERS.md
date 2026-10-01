# Fortnite 12.50 BR Solo gameplay status

> **Update 2026-10-01:** gameplay works. Erbium (Solo config, no GUI) hosts
> Apollo_Terrain on 7777 and the client joins via local matchmaking. See
> [MATCH.md](MATCH.md). The notes below are the earlier history.

Local backend login and a rendered Solo frontend work in the bounded diagnostic
profile. A real match, player spawn and gameplay do not work yet. The backend is
not the game server. Changing server projects does not evade security boundaries
or remove the need for a compatible game server and authenticated matchmaking.

## Actual first Play failure

Session `2026-09-30_01-37-58_b7a95cd1` rendered the Solo lobby and, after a verified
cursor-position synchronization fix, submitted an actual matchmaking request.
The client selected bucket `12895384:0:NAE:playlist_defaultsolo` and requested
`/fortnite/api/game/v2/matchmakingservice/ticket/player/:accountId`.
The local backend returned 404: no matchmaking route is mounted and no game
server is running. The game displayed the cannot-connect/unknown-error dialog.
This is the observed first Play failure, not evidence of an anti-cheat cause.

An independent profile-token request still returns explicit 501 because its
verification contract is not implemented. Do not manufacture a successful token
verification. The game exited before the router observation completed in this
run; helper exit 67 means this run was not a clean completed diagnostic, despite
the game process itself reporting exit 0. Owned processes were cleaned up.

Session `2026-09-30_02-08-27_2ea5ccc5` separately verifies that the client's observed
party preferences ALL/OPEN now return 204 instead of unsupported-config 501.
Those labels do not expose public party discovery or enable remote membership.
The local API remains authenticated, single-account and namespace-local. This
bounded run completed helper exit 0, no launch error and no remaining owned
processes. No EAC was observed in sampled monitoring in either run; that is not
proof of universal absence.

## Candidates inspected

The subsequent original backend adapter replaces that unmatched ticket 404 with
an authenticated HTTP 503, `fol.matchmaking.game_server_unavailable`. It enforces
the verified local token/account, reports the game-server component and refuses
to manufacture a ticket, signature or ready state. This improves diagnostics,
not gameplay. Authorization tests and the real isolated HTTP smoke verify the
contract; actual game-client retesting is recorded in PROGRESS.md.

| Project / pinned revision | Relevant evidence | Decision |
| --- | --- | --- |
| [Erbium](https://github.com/plooshi/Erbium), `44568e08cbe411ba197d3e0e900685750d46afdc` | README version range includes 12.50; Solo playlist default; Windows x64 DLL compiled and linked here | Primary buildable research candidate, not yet loaded or runtime-verified |
| [Nova](https://github.com/projectnovafn/Nova), `b6ee4d3121e4d01846f7ce0cc00946237bea3837` | Generic engine 4.25 logic; entry point compiled; full build missing OpenSSL/Detours; Nova service coupling | Reference pending dependencies, exact-version, licensing and initialization review; [analysis](NOVA_ANALYSIS.md) |
| [MGS](https://github.com/MarvelcoCode/MGS), `3f8b1e926ea3d0e19f00233ae23f09a403014614` | Requires supplied SDK and offsets; broad season claims are not exact 12.50 evidence | Not an immediately reproducible alternative |
| [Core](https://github.com/PongooDev/Core) | README lists full support only through 8.00 | Not the 12.50 candidate |
| [FortExternalServer](https://github.com/OGFN-Open-sourceing/FortExternalServer) | Documented 3.6/13.40 targets, not 12.50 | External EXE architecture does not establish exact-version compatibility |

Erbium's GPL-3.0 notice accompanies the generated research artifact. Only an
original build adapter is committed here. The full MSVC build produced
`build-erbium/44568e08cbe411ba197d3e0e900685750d46afdc/erbium-research.dll`, SHA256
`a448d5c8ceb78f9e9501e45646a2f76686e4d4f9e084ee3b2b2f2c07b2b1a8d6`.
It is a PE32+ x64 DLL, not a native Linux server. No module was loaded into a
game and no listen port was established by this build.

Source review found generic unconditional return/exit/kick and matchmaking
encryption patches in Erbium's initialization. We do not blindly activate them.
Its default cheats/siphon settings also do not match ordinary Solo. A compiled
artifact is not approval to load it. Any runtime experiment must preserve
security/admission requirements, isolate official services and document exactly
what reviewed gameplay-only functionality is enabled. Gameplay status remains
unverified until an actual owned private session joins, spawns and plays.

The next ordinary engine-interface experiment also failed to establish gameplay:
Apollo standalone/listen URLs started only Frontend in two real 12.50 runs. See
[world feasibility evidence](WORLD_PROBES.md). Generic map arguments alone do
not provide a working host here; other legitimate interfaces remain unverified.
