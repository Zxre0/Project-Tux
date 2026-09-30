# Isolated 12.50 SDK compatibility probe

This is an original GPL-3.0-only diagnostic, not a game server. It uses the SDK
headers from pinned Erbium `44568e08cbe411ba197d3e0e900685750d46afdc` but does not
compile its server entry point, gameplay hooks, exit/kick changes, permission
changes or encryption patches. SDK initialization scans the loaded executable,
initializes module-local offsets and uses engine allocation/name APIs. It is not
a zero-mutation memory reader, and does not certify upstream server compatibility.

The launcher requires isolated local Lawin login and the hash-checked local
router. Its bootstrap may load this exact additional module only into the game
child it created. There is no arbitrary PID/module-loading command. A dedicated
worker waits 30 seconds, checks 12.50/4.25 and records numeric reflection facts.
It neither executes gameplay functions nor starts a listener. Unexpected errors
and incomplete receipts are failures even if the runtime helper exits zero.

Build on the Linux host with the separately provisioned Microsoft toolchain:

```sh
unshare --user --map-root-user --net -- python3 integrations/windows/build_module.py \
  --provider erbium --sdk-probe --reference /absolute/path/to/Erbium \
  --toolchain /absolute/path/to/msvc-toolchain --jobs 1
```

The ignored output includes `fol-solo-sdk-probe.dll`, an adjacent version/role/
SHA-256 manifest and the upstream license. Add `--sdk-probe-module /absolute/path/
fol-solo-sdk-probe.dll` to the documented isolated Lawin diagnostic launch. It
cannot combine with startup `--world-probe` experiments. Never substitute the
unreviewed full Erbium research DLL.

## Actual experiments, 2026-09-30 UTC

Runtime: managed Proton Experimental 20260924, Wine 11.0, owned 12.50 prefix;
CachyOS/NVIDIA/Hyprland, supplied CL 13137020. Timeout 180 seconds, loopback-only
network namespace, normal local Lawin login. `OPENSSL_ia32cap` was unset; no
security checks or game files were modified.

- Session `2026-09-30_03-09-53_670abe91`, DLL SHA-256
  `d51006942af497989086079b335bb5585da39ce10731424c3df35450e524232e`:
  SDK init succeeded, versions 1250/425, object count 293948, process-event
  vtable slot 66, Athena/IP-driver classes and world found. ReadyToStartMatch
  lookup returned zero. Probe failure reported despite helper exit zero.
  Screenshot `sdk-probe-state.png` shows the signed-in mode chooser, not gameplay.
- Source diagnosis: `UObject::GetFunction` starts at the object's `Class`. Calling
  it on the UClass searches the metaclass. Upstream FortGameMode uses the default
  object. Our probe now looks up the function on the existing game-mode CDO.
  No signature, offset or game function implementation was changed.
- Session `2026-09-30_03-13-12_d1d88ae7`, corrected DLL SHA-256
  `6dbbfe35e668f3c9e156b7f8e9ed0d13d0414d1a3dcef90358d60b48a140315d`:
  all prerequisites passed, object count 293896, slot 66, ReadyToStartMatch found.
  Helper exit zero, no owned processes left; no EAC observed during sampled
  monitoring (not universal proof of absence). No server, join or spawn tested.

Native tests cover session/child binding, order/duplicate rejection, wrong
version, truncated/oversized receipts, partial stage retention, access-violation
reporting, private-payload exclusion, load ordering and mandatory isolation.
All 13 CTest targets passed after these changes (25.52 seconds).

Next: a separately bounded, game-thread-safe world-travel experiment. Successful
reflection does not prove that Apollo initializes, a UDP port binds, admission
works or a player can spawn.
