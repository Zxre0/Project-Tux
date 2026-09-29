# Troubleshooting

- `Build is not registered`: use `discover ROOT`, then `add PATH`. The repository
  does not download Fortnite assets. A missing build prevents any game launch.
- `Version unknown` / conflicting metadata: inspect `verify PATH` evidence. Do
  not rename the directory to pretend detection succeeded.
- Missing Vulkan headers during compilation: install developer dependencies or
  supply a pinned Khronos headers path to CMake. Runtime loader alone is insufficient.
- Missing Vulkan devices: inspect `diagnose` loader/instance/enumeration results,
  PCI drivers and distribution packages. GPU presence alone does not prove Vulkan.
- Prefix failure: the error identifies its initialization log. Only dedicated
  launcher-owned prefixes are permitted; unrelated Wine prefixes are never adopted.
- Interrupted runtime test: SIGINT/SIGTERM produces `interrupted: true` and a
  failed result after scoped cleanup. Inspect `owned_processes_remaining` and
  `processes.log`. Do not use global `wineserver -k`. SIGKILL cannot be caught.
- Runtime exits zero but test fails: inspect `execution_receipt_verified`;
  successful process exit alone does not prove the Windows command executed.
- Prefix recovery: stop all users, inspect `prefix backups ID`, archive any
  existing active directory, then `prefix restore ID TOKEN`. Restore never
  overwrites an existing directory or accepts incomplete backup metadata.
- Private-development path unverified: this is a compatibility blocker. An
  `--offline-probe` can investigate direct startup without network connectivity;
  it cannot provide authentication or private gameplay.
- Unknown failure classification: inspect the session's Wine and process logs.
  Classifications are evidence hints, not asserted root causes.
- EAC detected: the owned probe is stopped. Inspect process ancestry and launch
  chain. Do not modify, spoof or patch EAC.

Every launch request has `result.json` and `launcher.log`; planned commands also
have `launch-command.txt`. Executed probes add `system.json`, `wine.log` and
`processes.log`; graphics components may emit additional logs if active.
No empty placeholder logs are created to imply that components ran.

## Restricted test environments

The `network` CTest target requires loopback UDP socket creation and is labelled
`requires-network-sockets`. A sandbox that denies sockets should fail this test;
it is not converted to a success or skip. `ctest --test-dir build -E network`
runs the remaining targets, but is explicitly a partial suite, not a green full
build. The core target separately prints skips for namespace/GPU-dependent paths.
The full `./build.sh` and CI still run every target.

The separate `ipc` target similarly requires a working local AF_UNIX socket
bind/listen. In environments denying both local IPC and UDP, the explicitly
partial suite is `ctest --test-dir build -E 'network|ipc'`. Do not describe it as
a passing full build. A Wine `wineserver: bind: Operation not permitted` error
can occur without any GPU/display use; inspect `diagnose.local_ipc` before
resetting prefixes or attributing it to the game. Headless rendering is not a
substitute for functioning Wine-server IPC.

## Known sanitizer observations

The full graphics-enabled sanitizer suite reports retained allocations after
Vulkan enumeration on this CachyOS graphics stack (NVIDIA and Radeon, with Mesa
device-selection/anti-lag layers installed). Stacks include Vulkan initialization,
unloaded driver modules and libdbus. Before the execution restrictions changed,
100 non-GPU checks passed with leak checking enabled and 109 functional checks
passed in the normal build. This was **not** a
clean full sanitizer result; ownership of the graphics-related allocations is
still unresolved. No suppression or global driver modification is used.

In the earlier managed sandbox, LeakSanitizer itself reported a fatal error
(`does not work under ptrace`). ASan/UBSan still pass the socket-independent
suite with `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1` (98 core
and 32 manifest assertions). Leak detection was explicitly disabled for that
run; it makes no claim about leaks, graphics or socket-dependent functionality.
Five subsequent classifier regressions bring the portable core count to 103.
Native socket diagnostics and shared game-supervisor regressions subsequently
bring that count to 125 (plus 32 manifest assertions), with the same exclusions.
