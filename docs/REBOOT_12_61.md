# Historical Project Reboot Season 12 source map

Archived research scope: the original investigation focused on12.61 and nearby
12.x branches. As of2026-09-30, only12.50 is an active launcher target. Retain this
map as upstream source evidence; none of its12.61-only paths authorize12.50 logic.

Source pin: `Milxnor/Project-Reboot-3.0@3f6417ee6b07063bb6f83902ad3971f998888bab`.
All paths below are under `Project Reboot 3.0/` at that revision.
This is source evidence, not a runtime compatibility certificate.

## Explicit 12.61 paths

| File:line | Behavior | Consequence |
| --- | --- | --- |
| addresses.cpp:666 | Adds a version-specific pattern match to `toNull`; comment does not identify the function. A second Fritter crash pattern is commented out | Unknown function semantics; do not reproduce this patch or label it a verified workaround |
| FortGameModeAthena.cpp:49 | Event mode skips the normal event-playlist substitution for 12.61 | Generic Season 12 event setup is insufficient |
| FortGameModeAthena.cpp:1332 | 12.30 through 12.61 uses Donut bus/supply-drop assets | Gameplay asset selection is module-owned |
| FortGameModeAthena.cpp:1526 | Engine >=423 and Fortnite <=12.61 manually spawns vehicles | Version boundary matters; comment warns about post-S13 sync |
| dllmain.cpp:1313 | 12.61 uses native SpawnBot address hook instead of the UFunction hook used for other 12.00–13.40 builds | Do not generalize nearby boss-spawn initialization |
| FortServerBotManagerAthena.cpp:42 | Yacht-name branch and 12.30–12.61 select HDP bot data | Boss customization behavior differs by version |
| events.h:369 | Device event record for 12.61, Fritter loader/script callbacks and Fritter_High playlist | Event-specific configuration, not baseline startup proof |

`rg -n '12\.61|12\.60|12\.50|12\.41'` over the entire checkout found no
literal 12.60 or 12.50 paths. Additional 12.41 references occur in addresses.cpp,
finder.cpp, finder.h, FortGameModeAthena.cpp, dllmain.cpp and events.h. None should
be applied to 12.61 without independent evidence.

## Nearby 12.x and engine assumptions

`Addresses::SetupVersion` (addresses.cpp:23) reads an engine version string via a
scanned function or a release-string fallback. The latter initially assumes
engine 500; this fallback must not become our detection rule. Main initialization
at dllmain.cpp:930 runs version detection, offset discovery, then address discovery,
initialization and logging. The launcher reads supplied metadata without executing
these routines. Engine version for the actual supplied 12.61 build remains unknown.

For 12.61, source conditions in addresses.cpp:415–429 select Offset_Internal 0x4C,
Children 0x50, PropertiesSize 0x58; SuperStruct is engine-dependent (0x40 at >=422).
Func is 0xF0 **if engine 425**, while the 12.00–12.09 special case uses 0xC8.
PropName is 0x28 and Next is 0x20 (lines 534–539). These are upstream reflection
layout assumptions, not approved patch values for uninspected binaries.

ServerReplicateActors selects 0x5D for Season 12/13 only after earlier engine
branches have been considered (addresses.cpp:432 onward). Never choose it from
the season alone. NetDriver.cpp uses the pre-20 replication route, with >=422
and >=424 channel branches. `FFastArraySerializer::bNewSerializer` is enabled at
>=8.30. `reboot.h` parameter layouts branch around engine 424/425.

`finder.h` has engine-specific signature routes for 425 (including lines 735,
965 and 1343). The hardcoded 12.41 return at finder.h:1457 is not a 12.61 offset.
The exact unidentified 12.61 pattern is retained only in the pinned upstream
[addresses.cpp](https://github.com/Milxnor/Project-Reboot-3.0/blob/3f6417ee6b07063bb6f83902ad3971f998888bab/Project%20Reboot%203.0/addresses.cpp#L666).
Its first instruction bytes are `48 89 5C 24`; matching a prologue does not
establish function identity. No signatures, hooks, offsets or bypass behavior
have been introduced into the native launcher.

## Gameplay and remaining review surface

- Game mode: ReadyToStartMatch, HandleStartingNewPlayer, playlist, manual vehicles,
  abilities and event logic are in FortGameModeAthena.cpp. 12.00-only ability
  exclusions and 12.41 event handling do not automatically apply.
- Player/controller: FortPlayerController.cpp handles Wumba interaction at engine
  >=424 and version <15, new inventory rotation serialization at >=8.30, and
  >=11 zipline paths. FortPlayerControllerAthena.cpp uses component-owner casting
  at engine >=424. Player state remains module-owned.
- Inventory/pickups: FortInventory.cpp has older <7/<420 paths; FortPickup.cpp has
  <6 and >=23 branches. They contain no explicit 12.61 check. Absence of a special
  case does not prove compatibility. FortLootPackage.cpp:400 is 12.00-specific.
- Safe zone: FortSafeZoneIndicator.cpp forwards its hook and changes shrink
  timestamps. extra.cpp:25 selects the <13 late-game path, before later-version
  zone-duration array handling. Its generic 0x1F8 comment names S13/S14 and must
  not be assumed to describe 12.61 behavior.
- AI: bots.h permits newer bot logic at >=11 and engine <500; ai.h has a <13 path.
  Boss-spawn initialization must use the explicit 12.61 distinction above.
- Building, vehicles, inventory, commands and gameplay abilities stay in the
  server/module. commands.cpp has a <3 inventory command exception; exposing a
  command requires a verified module capability, not just an upstream string.
- FortGameStateAthena.cpp:359 uses an engine >=424 structure stride. Networking
  compatibility must be tested with matching client/module metadata.

The entire tree was searched for `12.`, `Fortnite_Version`, and `Engine_Version`.
This map covers all literal 12.61 occurrences and identified nearby branches;
it is not a complete semantic audit of every generic engine hook. Remaining work:
trace selected signatures against a legitimate 12.61 executable, review each
required module initialization dependency within the security boundary, and test
server/gameplay behavior. Upstream `bNoMCP` and auth-module dependencies do not
establish a legitimate non-EAC private execution path; no such path is verified.
