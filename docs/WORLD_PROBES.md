# 12.50 world/server feasibility probes

These are explicit isolated experiments, not a working host command. Normal
launch remains unchanged. `--world-probe standalone` requests `Apollo_Terrain`;
`--world-probe listen` requests `Apollo_Terrain?listen` as the game's first
positional argument. Both URLs live in the embedded 12.50 profile; arbitrary
maps/addresses/console commands are not accepted. The native CLI requires the
existing isolated Lawin local-login/identity profile and local router.

Hypothesis: the supplied Shipping build might expose the ordinary Unreal map URL
interface without a server module. This is described by Epic's
[UE4 client-server documentation](https://dev.epicgames.com/documentation/unreal-engine/client-server-model?application_version=4.27)
and [command-line documentation](https://dev.epicgames.com/documentation/es-es/unreal-engine/command-line-arguments?application_version=4.27).
Those generic engine docs are not a Fortnite 12.50 guarantee. Erbium's pinned
initializer selects Apollo_Terrain for Season 11 and later, including 12.50;
that supplies the map name, not proof of unmodified command-line support.

Use the documented bounded Lawin launch and append one of the above options.
`--print-command` verifies placement after Shipping.exe, before engine switches.
All processes remain in a loopback-only namespace; no new server/authentication,
anti-cheat, encryption or engine-check patches are added. Private login is not
replaced by map travel. Passing a map URL does not prove it was consumed.

## Observed results, 2026-09-30

- Standalone session `2026-09-30_02-39-25_2713b673`: game brought up
  `/Game/Maps/Frontend`, not Apollo. The screenshot shows the normal mode chooser
  signed in as Local Player. Local party/backend initialization still works.
- Listen session `2026-09-30_02-44-05_9a2375ce`: same frontend world, no Apollo
  world marker or game listen initialization in the captured log. A live socket
  check was attempted too late, after the process ended; no socket-absence claim.

Both 140-second bounded diagnostics completed helper exit 0, no launch error
and no remaining owned processes at their then-current revision. No EAC observed
in sampled monitoring, not proof of absence. Helper completion is **not** world
startup success. The later native result now compares recognized brought-up
world markers with the requested profile map and reports missing evidence as an
error, while retaining the underlying helper exit code. Query options are never
copied into analysis output because they can contain credentials.

The positional URLs do not establish a legitimate Apollo/server path in this
build. Do not repeat identical attempts or suppress its frontend/exit behavior.
Next inspect the alternative's SDK compatibility in a scoped diagnostic before
considering any reviewed gameplay hooks; do not load the generic upstream
security-disabling initializer.
