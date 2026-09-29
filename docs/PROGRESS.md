# Engineering progress

## 2026-09-28/29: milestone 1, native core and evidence collection

Canonical repository cloned to `/home/atlas/Projects/FortniteOnLinux`; verified
origin `https://github.com/Zxre0/FortniteOnLinux.git`. Repository had no commits
and no pre-existing files. No interrupted project processes were found. Existing
unrelated home-directory repositories and Wine prefixes were preserved.

Inspected Project Reboot 3.0 commit `3f6417ee6b07063bb6f83902ad3971f998888bab`
and Reboot Launcher commit `c6f82298b167ef2076bb59dc621f13dc5bd8d390` in separate
XDG cache reference checkouts. Read build-linux.sh and vcxproj, version detection,
explicit 12.61 branches, nearby 12.x paths, bot/game-mode/safe-zone initialization,
launcher process creation, metadata, controllers, config and dependency manifests.
Research notes identify the Windows PE/MSVC target and incompatible paused-EAC /
authentication-module chain. No upstream code or binaries were copied or patched.

Implemented native C++20/Qt core, CLI, TOML registration, embedded 12.61 profile,
PE and metadata validation, bounded build discovery, Wine/Steam Proton discovery,
dedicated prefix creation, Vulkan enumeration, local UDP bind diagnostics,
module PE/hash checks, offline diagnostic launch planning, scoped process
monitoring and failure classification. Functional scope and unimplemented features
are explicit in README. Private gameplay is not implemented or claimed.

Build failures and fixes: missing Vulkan headers; sudo required a password, so
used cached official Vulkan-Headers v1.4.304 without changing system packages.
Compiler rejected QString temporary-reference loops under -Werror; fixed the
loops. Qt command-option initializer overload was ambiguous; fixed option names.
`cmake --build build --parallel 4` succeeded. CTest: 3/3 passed, including 20 core
checks for metadata, corrupt/wrong-architecture PE, registry, prefix path safety,
EAC names, failure classification and persisted launch blockers. Synthetic assets
only; no Fortnite assets in tests or repository.

Actual diagnostics: CachyOS 7.2.8-1-cachyos, Hyprland/Wayland, Wine 11.18. Both
NVIDIA RTX 4070 SUPER and AMD iGPU enumerate via Vulkan. ELF32 Vulkan loader is
present but 32-bit device functionality is untested. UDP 127.0.0.1:7777 bind
succeeded. No EAC-named process observed in baseline snapshot. Proton Experimental
and Hotfix detected; no runtime comparison performed. DXVK/VKD3D are unconfigured.
Baseline report is outside Git at XDG state `logs/baseline-system.json`.

`build/fortnite-linux launch 12.61` failed in preflight: `Build is not registered:
12.61; use add /path/to/Fortnite`. Session `2026-09-29_05-03-35_b1924384`
contains result.json and structured launcher.log. No Wine/game process started.
Discovery under Games, /run/media and /mnt found no installation. Wider filesystem
search found only unrelated Alea and 14.60-labelled launcher artifacts. These were
not launched or modified. No root cause for actual Fortnite startup can be inferred.

Current task: exercise dedicated Wine prefix creation; then harden process and
metadata edge cases, run CLI integration tests, format, commit and push. Next
runtime milestone needs actual 12.61 files and a verified non-bypass private path.

### Prefix experiment and first checkpoint

`prefix create 12.61` initially reported that Wine exited without initializing
the prefix (log `prefix-4ce859aa-7715-4cb8-aa59-2df67bdcefa7.log`). Later inspection
found system.reg/user.reg and one surviving process: cleanup raced wineserver's
registry flush. Changed the implementation to wait for the selected prefix's
wineserver instead of terminating it immediately. Fresh `prefix create runtime-smoke`
then returned initialized=true and active_processes=0, using Wine 11.18.
Log: `prefix-f29e4565-571d-4c02-a59d-4d196a3a8c2f.log`. Initialization logged
Mesa EGL and Wine OLE/setupapi messages; no inference of game rendering success.
Both dedicated prefixes remain outside the game directories and Git. No game files
or unrelated prefixes were removed. Prefix initialization is now tested on this host.

Also made printed commands include the actual managed environment overrides and
Wine log redirection, and prevented a busy monitoring loop after the initial
Wine process exits. Rebuilt: CTest 3/3 passed. Formatted all C++ source. Preparing
first commit and push; next task is adversarial metadata/process regression testing
and CLI subprocess integration coverage.

### Second iteration: metadata boundary and process ownership

First checkpoint `f4463ef` pushed successfully to origin/main. GitHub Actions run
36524773798 passed on Ubuntu 24.04, including formatting, compilation and tests.
Initial commit attempt lacked Git author identity; configured only this repository
using the authenticated owner's GitHub noreply identity, then committed/pushed.

A new regression test reproduced a chunk-boundary parser error: a release string
split after `12.6` was incorrectly collected as a second version. Require a real
metadata terminator and distinguish read boundaries from EOF; regression now passes.
Malformed TOML registry schemas now fail without replacing the file. Added
synthetic process tests proving EAC-name detection, session/prefix ownership,
stale start-time rejection, and survival of an unrelated child process.

Added `prefix stop ID`, guarded by ownership marker and operation lock; uses
pidfds and bounded termination, not a global Wine kill. Tested on the orphaned
winedevice from the first prefix experiment: stopped_process_count=1 and
active_processes=0. The successfully initialized runtime-smoke prefix already
had zero processes. No prefix directories or game files were deleted.

Expanded regression coverage to CLI subprocess registration/removal/verification,
failure exit codes, module manifest/hash corruption and occupied UDP ports.
Private startup and game compatibility remain blocked by missing 12.61 files.
Validation after these changes: 42 core/integration assertions passed; CTest 3/3.
Next: exercise actual Wine execution inside the offline namespace and record its
limits separately from Fortnite startup, then extend dependency evidence.
