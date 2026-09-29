# Engineering progress

## Current handoff state

Fortnite 12.61 files remain unavailable. The user supplied a separate 12.50 build;
its metadata, structure and all 398 local-manifest file hashes are verified.
GPU/socket restrictions were removed by the user. The opt-in isolated 12.50
launcher-context probe now reaches a visually verified sign-in screen using
Proton Experimental/DXVK on CachyOS/NVIDIA/Hyprland. Baseline arguments still exit
early; distinguish those historical runs from the context experiment. Steam Linux
Runtime container integration is not implemented. No login, lobby, private join,
hosting or 12.61 gameplay is verified.

Private-backend discovery narrowed the next blocker: MCPCONFIG=localhost did not
change selection; epicapp=Localhost is recognized but lacks the game's service
configuration and is rejected with host unconfigured. Do not interpret this
known-negative opt-in as a working private launch path. Latest verified release
suite passes 8/8 targets, including 177 core checks and 15 game-evidence checks.
Historical sanitizer results below are scoped to their exact revisions and
settings; graphics-enabled LeakSanitizer findings remain unresolved. Findings
through b8406b7 are pushed and GitHub CI passed (run 36615515180).

Installed native CLI: `/home/atlas/.local/bin/fortnite-linux`. Source checkout:
`/home/atlas/Projects/FortniteOnLinux`. Dedicated prefixes `12.61` and
`runtime-smoke`, and `12.50` are initialized. The corrected run left no owned
12.50 processes. Game binaries, GPU drivers and boot configuration have not been
modified. The earlier process-scope defect may have affected unrelated user
applications; see its explicit incident record below.
Recoverable Wine-prefix backup/reset/restore has been tested on real prefixes.
Project-owned Experimental and Hotfix copies passed isolated command execution
and clean-shutdown tests using separate Proton compatibility data. Cancellation,
timeout, missing-receipt and EAC-name handling have synthetic regression coverage.

An accessible complete 12.61 installation remains required for 12.61 testing.
The 930.5 GiB NTFS Windows partition `/dev/nvme1n1p3` is unmounted. A read-only,
noninteractive UDisks mount attempt was denied with `NotAuthorizedCanObtain`.
It was not mounted or inspected, and no filesystem repair/write was attempted.
It may contain the missing game files; their presence/version is unknown.

Current task: structured game-log diagnostics and regression tests, followed by
the unresolved private-service configuration contract. The successful context-only
sign-in baseline remains available. Reboot source search found no MCPConfig
configuration setting, and the supplied installation has no loose relevant INI
file. Neither result proves no supported configuration route exists. Do not
blindly repeat known-negative arguments, start paused EAC stand-ins, contact
protected production services or treat offline sign-in as private gameplay.

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

### Third iteration: Wine execution and complete supervisor fixture

Second checkpoint `f7a642c` pushed; Ubuntu CI run 36525062014 passed.
Added `runtime test PREFIX_ID`: built-in cmd.exe execution within an offline
namespace, with session logs and cleanup verification. First attempt executed
cmd.exe successfully (Windows 10.0.19045) but returned failed cleanup. Inspection
showed six services still running despite a successful host-side wineserver wait.
Hypothesis: mapped UID changes Wine's server socket identity, so wait outside the
user namespace targets a different server. Moved wait inside the same namespace.
Retest passed with no owned processes. Failing session:
`runtime-20c5f10a-59d5-4cff-b833-f9417e7c2b5b`; passing session:
`runtime-fcfb4677-b274-4b7c-a8cc-af24990951e1`. Both retain Wine logs. This is a
runtime smoke test, not Fortnite startup evidence.

Added a native synthetic runtime fixture and exercised the complete offline launch
supervisor: command planning, captured exit 53 and missing-library classification,
EAC-named synthetic child detection, scoped termination, and zero surviving owned
processes. No Fortnite or EAC binary is involved. Namespace/Vulkan-dependent
fixture checks explicitly skip on unsupported CI hosts; other tests always run.
Hardened metadata against special-file paths and prefix placement inside an
unregistered structurally recognizable Fortnite installation. Embedded resources
are initialized once. Local CTest 3/3 passed including 47 assertions.

Both dedicated prefixes now report initialized=true and active_processes=0.
Current blockers remain unchanged: no complete 12.61 installation, no validated
private non-EAC gameplay path, no reviewed compatible module/backend. Next task:
inspect prefix components and actionable package evidence, then finalize the
current reproducible diagnostic baseline before actual 12.61 runtime testing.

### Fourth iteration: content hashes and dependency evidence

Third checkpoint `dfdf895` pushed; Ubuntu CI run 36525509616 passed. Added optional
streaming SHA-256 integrity verification from user-supplied TOML manifests. Tests
cover matching content, corruption, path traversal and symlink escape rejection.
No content download or mutation is performed. Prefix status now inspects five
graphics/VC++ DLLs without conflating their presence with native component setup.
Actual 12.61 prefix has five valid x64 PE DLLs; providers remain unverified.

`diagnose` now reports installed Arch-family package evidence for Wine, Vulkan,
Mesa and detected GPU vendors, and suggests missing package names. Compared local
package versions with official Arch package pages. Both 64/32-bit vendor packages
are installed here, but only 64-bit Vulkan enumeration has been functionally tested.
Latest report saved outside Git as `logs/latest-system.json` in XDG state.

Local build and CTest pass: 51 assertions including full synthetic supervisor
checks. Running a separate AddressSanitizer/UndefinedBehaviorSanitizer build to
check the binary parser and process/CLI tests before the next push.

Sanitizer result: all 51 functional assertions completed, but full CTest failed
LeakSanitizer with 6,397 bytes in 48 allocations. Reproduced via standalone
`diagnose`; extended stacks include vkCreateInstance, unloaded graphics modules
and libdbus. `build-sanitized/core-tests --skip-gpu` passes 46 assertions with
ASan/UBSan/leak checking enabled. Controlled per-process driver-selection runs
using `VK_DRIVER_FILES` retain 5,641 bytes for NVIDIA and 768 bytes for Radeon
after 51 assertions pass. These results localize the observation to Vulkan
driver/layer initialization, but do not prove which library owns each allocation.
No sanitizer suppression, driver change or gaming environment override was added
to normal launch behavior. Full report remains in
`build-sanitized/Testing/Temporary/LastTest.log`; unresolved graphics-stack leak
is documented in TROUBLESHOOTING.md. Full sanitizer suite is **not** reported green.

Next game-runtime work requires a complete legitimately supplied 12.61 build.
Filesystem discovery and upstream inspection cannot supply those assets or prove
a legitimate non-EAC private gameplay path. No game startup/menu/join/hosting
success, native VC++ deployment, DXVK/VKD3D setup, or GUI completion is claimed.

### Final verification and environment blocker

Checkpoint `e37c201` pushed; Ubuntu CI run 36525961827 passed. Local release build:
CTest 3/3 and 51 assertions passed. Sanitized non-GPU tests: 46 assertions passed;
full graphics-enabled leak result remains unresolved as documented above.
Installed the verified CLI using CMake to the previously absent user-local target;
ran `version` and `builds` from `/tmp` successfully (registry empty). No installed
binary was overwritten. Documentation includes user-local installation steps.

Storage discovery found the unmounted Windows NTFS volume. Command attempted:
`udisksctl mount --block-device /dev/nvme1n1p3 --options ro --no-user-interaction`.
Result: `org.freedesktop.UDisks2.Error.NotAuthorizedCanObtain: Not authorized to
perform operation`. Sudo also requires interactive authentication (earlier header
installation attempt). No supplied game installation is accessible in the current
environment. Real startup diagnosis cannot continue until the installation becomes
accessible; screenshots, simulated assets and runtime smoke tests cannot substitute
for it. No background development process or game is left running.

### Continued iteration: Git recovery and prefix lifecycle

On resumption, HEAD `9be15ab` could not be read: six loose Git objects were empty.
Cause is unknown. Remote HEAD still matched that commit. Preserved the entire
local `.git` under the XDG cache `git-recovery-20260929/original-git`, cloned a
read-only recovery mirror, verified it with `git fsck --full`, copied its pack,
and quarantined only the six confirmed zero-byte objects. Working files and
refs were not reset or overwritten. Local `git fsck --full` now passes and the
worktree was clean before new edits. Corrupt originals remain recoverable in cache.

Implemented prefix copy backup, listing, archive, reset and explicit restore in
a separate core module. Operations reject unowned/active/locked prefixes and
symlinked storage; copy preserves Wine host links without following them.
Restore never overwrites an existing prefix; reset archives before initialization.
Partial backups stay on disk with incomplete status. No permanent deletion occurs.
Regression tests cover preservation, ownership, locking, symlinks, traversal,
enumeration and active processes. Release CTest: 3/3, 66 assertions. Sanitized
non-GPU suite: 61 assertions passed; earlier graphics-stack leak remains unresolved.

Actual `runtime-smoke` prefix: copied backup, reset/reinitialized, passed offline
Wine cmd test (`runtime-2ee2ef82-3407-4e66-9b73-5f582ef1dbc7`), archived replacement,
restored original copy, passed again (`runtime-458f9ff1-12bc-4ac8-81a1-2c2ec2f537f0`).
Both tests exited 0 with no remaining owned processes. Two archived prefixes and
the consumed-copy metadata remain in XDG backup storage; nothing was deleted.

Next: make Proton smoke testing reproducible. Inspected installed Experimental
script setup, default-prefix creation, SteamPipe fixups, compatdata layout, logging
and run/runinprefix paths. Script may mutate its runtime directory, so experiments
use a project-cache copy, not Steam's installation. Initial isolated `run cmd.exe`
exited 0 and loaded cmd.exe, but stdout was not captured; do not yet treat that as
a proven command result. Investigating an explicit per-session execution receipt.
No 12.61 assets or legitimate private gameplay integration have become available.

### Proton runtime import and reproducible smoke tests

Prefix checkpoint `5069993` pushed; Ubuntu CI run 36526918663 passed.
Added trusted installed-runtime import into project data, leaving Steam's original
unchanged, and `runtime test-proton ID --proton RUNTIME_ID`. Runtime copies have
ownership and script/version digests; existing IDs cannot be overwritten. Prefixes
are isolated from Wine data and tied to a single imported runtime. Baseline tests
reject custom Python user settings and clear inherited Steam/Proton/Wine/media
overrides. Source inspection found media migration paths that can mutate files,
so this clearing is a storage-safety measure as well as a reproducibility measure.

Proton's Steam shim did not relay cmd console output in the first manual run.
Implemented a unique Windows-written per-session receipt rather than inferring
success from exit 0. The supervisor requires that receipt, normal exit 0, no
observed EAC-name process, and complete process drain. It handles interrupt,
timeout and scoped cleanup. Proton's trailing WINEPREFIX slash exposed an exact
string-matching issue; normalized absolute lexical paths now match correctly
without admitting another prefix with a similar name. Synthetic tests cover
successful execution, stale/missing receipt with exit 0, EAC-named child cleanup,
runtime mismatch/digest changes and preservation of the import source.

Actual results, all using offline user/network namespaces and no game files:
- Experimental 11.0-20260924: receipt/exit/cleanup passed in sessions
  `proton-798a8e51-41a3-47c1-b84b-bf47bf5099e3` and
  `proton-79f9a9eb-cf4e-42bb-bad0-9da8edfa95b6`.
- Hotfix 20260828-ptr: receipt/exit/cleanup passed in
  `proton-922539ad-8be4-40fb-a037-607d15cb26aa`.
- Wine baseline remains intact; imported runtimes are not selected for game launch.
- Release CTest 3/3 and 81 assertions passed; ASan/UBSan non-GPU suite 76 passed.
  Full graphics-enabled sanitizer leak remains unresolved, not declared green.

Proton logs retain winebth/service and Steam-connect errors. Empty project Steam
client path is intentional; no account or production login is needed for cmd.exe.
No game-specific compatibility or rendering conclusion follows from these tests.
Next: review failure/interrupt cleanup and strengthen runtime diagnostic reporting
while actual Fortnite 12.61 startup remains blocked by inaccessible game files.

### Runtime interruption and descendant cleanup

Proton checkpoint `d9719ea` pushed; Ubuntu CI run 36527549548 passed. Review found
that the older Wine smoke path used a blocking subprocess helper without the
Proton test's interrupt handling. Extracted shared runtime supervision, including
SIGINT/SIGTERM handling, timeout, timestamped process-tree snapshots, pidfd cleanup,
and verified descendant ownership retained across reparenting. Zombies are not
counted as live processes. PID reuse remains guarded by recorded start ticks.
Proton and Wine both use this module; the game-launch supervisor is unchanged.

Added CLI subprocess tests that wait until a synthetic runtime is actually running,
send SIGTERM, then require structured failure and no owned survivors. Added timeout
cleanup and synthetic EAC-named descendants with WINEPREFIX/session removed;
ancestry still detects/stops them. No anti-cheat code is executed. Wine now requires
a fresh Windows-written execution receipt as well as exit status, matching the
Proton false-success safeguards. Command evidence is saved before starting Wine.

Initial rebuild caught Qt's `signals` macro conflicting with a local variable name;
renamed it to `signalScope`. Retest: CTest 3/3; 91 assertions; ASan/UBSan non-GPU
86 assertions. All passed. Real command retests with shared supervision also passed:
Experimental `proton-3c06beb2-9ae6-4696-b0e7-e4a8bbeb31b4`, Hotfix
`proton-f57d6a61-20e6-45d8-8466-75f7a44c3af7`, and Wine with the new receipt
`runtime-0f3089e0-b2b5-406a-b3dd-03aacc3a93e4`. No owned processes remained.

Rechecked accessible game roots `/home/atlas/Games`, `/mnt`, `/run/media/atlas`:
no installation candidates. NTFS volumes remain unmounted. Explicit startup attempt
`launch 12.61 --offline-probe --timeout 10` correctly stopped at registration
preflight: `Build is not registered: 12.61; use add /path/to/Fortnite`.
Session `2026-09-29_05-49-23_325daeb5` retains the blocked result. No game or EAC
was started. An accessible legitimate 12.61 installation is still the prerequisite
for the next real startup experiment; runtime tests cannot replace this evidence.

### Supplied 12.50 build and critical process-scope correction

User supplied `/home/atlas/Documents/Fortnite/++Fortnite+Release-12` and requested
continued autonomous development. Embedded metadata in both Shipping and
CrashReportClient identifies **12.50**, not 12.60/12.61. Shipping's UTF-16 metadata
includes CL 13137020. Engine version remains undetected. Structural validation
passes; this is not a full content-integrity or authenticity assertion.
Added a separate embedded 12.50 diagnostic-only profile; ordinary private launch
remains gated. 12.61 stays the default, and its module/version assumptions are
not applied to 12.50. Upstream search found no exact `12.50`/`12.5` branches in
the inspected Reboot source; neighboring version logic is not proof of support.

Inventory includes ordinary Shipping, Shipping_EAC, Shipping_BE, FortniteLauncher,
EAC setup/DLLs and BattlEye service/client DLLs. Bootstrap configs target ordinary
Shipping. No bootstrap, authentication module or protection patch is used. Added
BattlEye process detection and Wine module-load evidence monitoring alongside EAC
process detection. A positive module-load trace stops the game probe; this is
reactive observation, not a guarantee that no module briefly loaded.

Registered 12.50 and initialized its dedicated Wine prefix. First 45-second offline
probe session `2026-09-29_06-13-15_5c4ab67f` produced Wine logs showing engine exit
and LauncherCheck shutdown, but no result.json. On continuation its process handle
was absent and no related processes remained, so no exit code can be asserted.
Inspection found a **critical launcher process-selection bug**: QProcess reports
PID 0 after exit; the old ancestry scan seeded that value, reaching init/kernel
roots and unrelated sessions while Wine services remained. Captured snapshots
confirm expansion to 425 processes at 06:13:29.876 UTC. Cleanup may have signalled
unrelated user processes and the launcher itself. The exact external impact is
unknown; it is not correct to claim this run preserved unrelated applications.
No new real-game run was attempted before fixing this issue.

Implemented shared identity-checked tree selection that rejects PID 0/1 roots;
both game/runtime monitors use it. Signalling also rejects PID 0/1, the launcher
and its ancestors as defense in depth. Regression tests include a root that exits
before a detached Wine-style child, an unrelated live child that must survive,
stale root identity, and zero/init roots. Release build/CTest 3/3 and 109 assertions
pass. Next: sanitizer regression, commit this safety correction, then capture a
correctly supervised 12.50 startup and its exact visible failure.

### Corrected baseline and restricted-environment blocker

Checkpoint `1e6ba64` was committed/pushed and installed before the corrected game
attempt. Its full release suite passed 109 assertions and non-GPU sanitizer suite
100. Session `2026-09-29_13-35-35_66361806` now contains a completed result: game
exit code 1, no EAC/BattlEye observed, unknown failing subsystem. Process scope is
bounded to at most nine processes across 165 samples, with no system roots. No
active prefix processes remained. Log shows LauncherCheck/LauncherPlatform teardown
and Wine/EGL warnings; neither is sufficient evidence of the exit's root cause.

Added explicit `--trace-startup` to collect Wine msgbox/debugstr diagnostics without
changing game arguments, credentials, modules or networking. Both traced and default
`--print-command` plans were validated against the actual registered 12.50 build.
The Shipping SHA-256 still matches the initial inventory exactly.

The execution permissions changed to a managed restricted sandbox. Current launch
session `2026-09-29_13-37-40_7e950a34` fails Vulkan preflight: instance result 0,
enumeration result -3, zero devices. PCI/sysfs still lists both GPUs, but DRM/NVIDIA
device nodes are absent. Direct AF_INET/UDP socket creation independently returns
EPERM. Build compiles, but CTest core fails at `test UDP bind`; CLI help/version
pass. This is an environment restriction, not evidence of a 12.50 regression.
No gate was disabled and no privileged/sandbox-escaping execution was attempted.
Vulkan diagnostics now report visible device nodes and an explicit access hint
when none are visible, rather than suggesting host driver changes blindly.

GitHub API access also currently fails. Continue keeping local commits and record
push results. Next real experiment remains the unchanged isolated game command
with targeted startup tracing in an environment with GPU/display/socket access.
The no-bypass boundary remains in force if that trace reveals an authorization
or anti-cheat requirement. No game menu, private join, hosting or 12.61 success.

### Read-only 12.50 content verification during sandbox restriction

Startup tracing/device diagnostics were committed locally as `8836f44`.
`git push origin HEAD` failed with `Could not resolve host: github.com`; the
commit remains local. No work was discarded and no force push was attempted.

Added independent bounded legacy Epic manifest inspection and file verification,
with CLI `manifest PATH --installation ROOT [--hash-files]`. No downloads,
decryption, repairs, game writes or manifest-provided command execution.
Initial strict parser rejected metadata feature 13 despite container 18; source
format inspection confirmed those are distinct fields. Added explicit 13–18
feature support while retaining v17/18 container and v0 file-list bounds.
New zlib dependency is explicit in CMake and CI.

Supplied manifest identifies `++Fortnite+Release-12.50-CL-13137020-Windows`.
Size check and then full SHA-1 check both passed: **398/398 files**, zero missing,
zero mismatched, **90,166,901,135 bytes hashed**, elapsed **72.658 seconds**.
Report: XDG state `logs/12.50-manifest-hashes.json`. Internal manifest SHA-1
also matches. The local manifest is not authenticated, so authenticity is not
claimed. Metadata names FortniteLauncher.exe, but no change to the ordinary
Shipping-only offline launch chain was made. No game assets were modified.

Release build and 32 dedicated asset-free manifest assertions pass; malformed
compression/sizes, corruption, optional tags, empty files, UTF-16, traversal,
duplicates and symlink escapes covered. AddressSanitizer/UBSan pass these 32
checks with `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`.
LeakSanitizer itself fatally fails under this sandbox (`does not work under
ptrace`); this is not a leak-free claim. Current full CTest: 3/4 pass, core still
fails its UDP-bind assertion. Formatting/diff checks pass.

Next: separate network-dependent tests from portable core regressions, retaining
the network failure explicitly. Real traced startup still needs GPU/socket access;
the file-integrity result does not explain the Wine exit or prove gameplay.

### Separate network preconditions from portable regressions

Manifest work committed as `966527c`, installed user-locally. Push retried and
failed again (`Could not resolve host: github.com`); both new commits remain local.
Moved UDP fixture/occupied-port assertions out of the monolithic core target
into a dedicated network target, adding ephemeral-bind and no-router-change
assertions. No existing failure was weakened or automatically skipped. CI and
build.sh still run all tests. Core now reaches 98 passing assertions in this
environment; manifest passes 32. Full CTest is 4/5 with network failing, while
explicit `-E network` subset passes in release and ASan/UBSan builds. GPU-dependent
core paths print their existing skip rather than claiming runtime coverage.
Leak detection is disabled only for the recorded sandbox sanitizer run because
LeakSanitizer itself fails; no suppressions were added to project configuration.

Reviewed corrected Wine log again: RPC exception is handled and engine teardown
still supplies no decisive cause. Next useful offline task: tighten failure
classification so unrelated Wine errors cannot falsely blame DXVK/VKD3D merely
because their names occur elsewhere in the log. Real startup remains blocked.

### Prevent misleading graphics failure classification

Network-test separation committed as `e2edb4f`. Reproduced the classifier defect
with a failing regression: `info: DXVK version 2.0` followed on another line by
an unrelated Wine service error was incorrectly classified as DXVK. Changed only
the graphics hint matching to require a same-line component name and severity
token, instead of finding `err` anywhere in the entire log. Added five assertions
for unrelated errors, mere module presence, non-severity substrings and positive
component-specific errors. The corrected real Wine log's cause remains unknown;
no startup arguments, protections or runtime binaries changed.

Release and ASan/UBSan socket-independent suites pass: 103 core + 32 manifest
assertions, leak detection explicitly disabled for the sandbox run. The network
target remains failing; graphics-dependent supervisor checks remain skipped.
Next: improve native network diagnostics for socket-creation denial, since Qt
currently reports only `Unknown error` for this environment's EPERM restriction.

### Actionable socket-creation diagnostics

Graphics classification committed as `b61e0c0`. Added a Linux-platform UDP socket
creation probe after failed Qt binds; it records the actual errno and operation
without claiming successful creation means successful binding. It closes any
created descriptor and sends no traffic. `network 127.0.0.1 7777` and
`network ::1 7777` now both report creation denied with **errno 1 / Operation not
permitted**, preserving Qt's bind error and distinguishing it from an occupied
port or remote firewall failure. No permissions, firewall or router changes.

Four capability-report assertions bring the portable core count to 107; all 32
manifest assertions continue to pass. Release full CTest remains 4/5, with the
network failure now showing concrete native evidence. ASan/UBSan partial suite
passes with the previously documented leak-detection restriction. Formatting and
diff checks pass. This improves the blocker report, not game compatibility.

Remaining startup experiment: targeted msgbox/debugstr trace of ordinary 12.50
Shipping, unchanged `-log -d3d11`, isolated network, bounded supervisor and no
anti-cheat bootstrap. Current sandbox lacks GPU nodes and denies socket creation;
do not disable either safety preflight or isolation. Local manifest verification
has exhausted the supplied installation's ordinary integrity question. Next
safe work is a focused audit of preflight/process-monitor regressions and parser
bounds; another identical game launch in this unchanged environment adds no
startup evidence. Push remains subject to GitHub DNS availability.

### Consolidate game and runtime supervision

Socket diagnostics committed/installed as `4ef0f09`; push retried and again
failed with `Could not resolve host: github.com`. Local commits are preserved.

Source audit found the separate game monitor lagged behind the shared runtime
supervisor: unchecked process-log writes, repeated observed-process entries,
no retained ancestry for environment-stripped/reparented children, and no final
remaining-process report. Removed that duplicate loop. Game probes now use the
existing shared identity-scoped supervisor with explicit working directory and
optional bounded module-load trace inspection. Game launch/profile/isolation
preflights are unchanged. Unreadable traces and failed process logs stop the
owned probe; result records remaining members and QProcess crash status. Signal
handlers use the shared scope guard. EAC wording says stop requested and refers
to the cleanup result rather than claiming unconditional termination.

Added synthetic native fixtures only (no Fortnite/EAC code) for EAC/BattlEye-like
module log lines, a 1 MiB read-boundary trace, harmless module-name mentions,
working-directory preservation, an observed child reparented after removing its
ownership environment, and process-log write failure via `/dev/full`. The initial
orphan fixture used Qt's double-fork detached mode, which reparented before it
could be sampled; that fixture correctly failed the intended ownership assertion.
Changed the fixture to a normal observed child followed by parent `_exit`, testing
the stated sampled-ownership guarantee without claiming unseen-child coverage.

125 core assertions now pass, repeated **10 consecutive runs** (18.75 seconds).
32 manifest assertions remain passing. ASan/UBSan socket-independent suite passes
with leak checks disabled as previously documented. The actual 12.50 traced launch
plan still selects ordinary Shipping and includes `--net`; status is explicitly
`planned; not executed`. No new real-game execution or graphical success is claimed.
The full network test remains blocked by EPERM. Formatting and diff checks pass.

Next: capture the targeted startup trace once GPU devices and sockets are available.

### Continuation: startup blocker revalidated after supervisor checkpoint

The preceding goal turn made verified engineering progress: manifest verification,
diagnostic corrections and shared game supervision. This continuation rechecked
the authoritative worktree and environment before attempting further work.
HEAD is `b2602a4`, installed user-locally; the worktree was clean. Known remote
tracking checkpoint remains `1e6ba64`, six implementation commits behind HEAD.
Another `git push origin HEAD` failed with `Could not resolve host: github.com`.

Fresh `diagnose` report saved outside Git to XDG state
`logs/restricted-environment-check.json`: no GPU device nodes, no Vulkan devices,
enumeration result -3; native IPv4 UDP creation fails errno 1 / EPERM. Full CTest
again passes 4/5 targets and fails only the network target. Existing hash report
still records 398 matching files; the last completed real game session still
records exit 1 with unknown failing subsystem, not menu/gameplay success.

No repeat game start was issued: these preflight restrictions prevent the same
next experiment before it reaches Fortnite. No environment bypass, privilege
change, disabled validation gate or host-driver modification was attempted.
There is no new game/runtime evidence supporting another compatibility patch.
Further generic polish is not a substitute for the actual startup objective.

Required next external-state change: a permitted execution environment with GPU
device/display access and socket creation, plus GitHub connectivity for pushing.
Then rerun `fortnite-linux diagnose`, the full tests, and
`fortnite-linux launch 12.50 --offline-probe --trace-startup --timeout 35`.
Retain the network namespace, ordinary Shipping executable, unchanged game args,
anti-cheat observation and scoped cleanup. Inspect the resulting message-box /
debug-string evidence before proposing a compatibility change. If it proves an
authorization/protection requirement, document that blocker rather than bypass it.
No completion claim is made; the objective remains unmet.

### Resumed work: test headless runtime alternative, identify Wine IPC denial

User requested continued work after the blocked checkpoint. Rechecked clean
canonical checkout at `2c9defa`. Investigated whether a GPU-independent runtime
experiment could advance startup diagnostics without changing the security
boundary or bypassing sandbox restrictions.

Executed the existing isolated Wine command test on the owned `runtime-smoke`
prefix with DISPLAY and WAYLAND_DISPLAY removed. New session
`runtime-4ad5f6d6-464a-46ac-ad2c-03036090db01` failed immediately: exit 1, missing
Windows command receipt, `sock_init: ERROR in sock_check_pollhup()` followed by
`wineserver: bind: Operation not permitted`. No Fortnite or anti-cheat executable
was selected; post-test prefix status reports zero active processes. The failure
occurs before GPU rendering and blocks even the ordinary Windows command probe.
No new headless game profile or changed game arguments were introduced.

Epic's [command-line reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-command-line-arguments-reference)
documents null-RHI/headless operation generically, not verified support for this
Fortnite build. A renderer option cannot fix Wine's earlier IPC failure, so no
such argument was treated as an established Fortnite compatibility workaround.

Added independent `diagnose.local_ipc` AF_UNIX stream socket creation/bind/listen
probe with operation, errno and scoped temporary cleanup. Actual result: socket
creation succeeds, **bind fails EPERM**, matching the Wine-server error. This is
more specific than the earlier IPv4/IPv6 socket-creation evidence. No Wine socket,
game asset, active prefix, sandbox policy or GPU driver was modified.

Added a separate IPC CTest target that requires real success, plus portable
report/cleanup assertions. Current full release CTest is **4/6**: core, manifest,
CLI help/version pass; network and IPC targets fail explicitly under restrictions.
The next real launch still requires permitted local IPC and GPU/display access;
host networking alone would not fix Wine's local bind failure.

Validation: 128 portable core assertions and 32 manifest assertions pass. The
explicit ASan/UBSan subset excluding `network|ipc` passes with leak detection
disabled under the already documented sandbox restriction. Formatting and diff
checks pass. Fresh diagnostics saved to XDG state `logs/headless-ipc-diagnostics.json`.
Next required action remains a permitted environment with working AF_UNIX IPC;
only then can Wine startup and the targeted Fortnite trace proceed meaningfully.

### Continued static startup dependency investigation

User requested continued autonomous engineering. IPC/GPU/UDP restrictions remain
unchanged. Read-only `git ls-remote origin HEAD` still fails GitHub DNS. Checked
the separate available GitHub connector as an authorized publication alternative:
`get_repo` for `Zxre0/FortniteOnLinux` returns 404. That does not prove the repository
is absent; it may not be available to the connector. No external writes attempted
through that inaccessible connection. Previous IPC diagnostics are committed as
`c7e7a97` and installed; their normal Git push failed DNS as well.

Inspected real Shipping PE imports with GNU objdump and LLVM. Implemented native
`imports PE_PATH [--dll-dir DIRECTORY]` using the documented PE structures, not
an executable loader. Results: 50 immediate and 17 delayed DLL names, independently
matching LLVM exactly. All immediate imports have candidates in the game directory
or dedicated prefix system32. Nine delayed imports absent from those two locations
were found in the supplied third-party subdirectories; see DEPENDENCIES.md.

Inventory caught a real architecture distinction: CEF3/Win64/d3dcompiler_43.dll is
x86, while the prefix system32 counterpart is x64 (`file` confirms both). This
is not a proven startup cause: the corrected Wine log contains no load evidence
for d3dcompiler/libcef. Kept both untouched. Every import has a candidate when
explicit third-party directories are inventoried; this is not proof of Wine
resolution, exported-symbol compatibility or transitive dependencies.

Parser tests cover immediate/delayed descriptors, bounds, null terminators,
overlap, names, case-insensitive candidates, API sets and wrong architectures.
The null-name test initially accidentally built a null descriptor (a valid table
terminator); corrected the synthetic fixture to retain its thunk fields before
testing a null name. 24 assertions pass. Core 128 and manifest 32 continue passing.
ASan/UBSan portable subset passes with leak detection disabled as documented.
Full release CTest remains honestly 5/7: IPC/network operations are denied.
Reports are outside Git under XDG state `logs/12.50-import-inventory*.json`.

Dependency diagnostics committed/installed as `6f4b634`. Updated complete inventory
confirms all 67 names have at least one valid x64 DLL candidate in the explicitly
inventoried directories; the x86 CEF compiler candidate remains reported separately.
Push retried and still fails GitHub DNS. `git fsck --full` passes. A verified
recovery bundle preserves the nine pending commits through `6f4b634` at XDG state
`checkpoints/pending-6f4b634.bundle`; it requires base commit `1e6ba64` and is local
recovery material, not an off-machine backup or successful publication.

### Restored permissions: real startup tracing resumed

User restored local execution access. Native AF_UNIX bind/listen and UDP 7777
binding now pass; NVIDIA RTX 4070 SUPER and AMD iGPU Vulkan enumeration succeeds.
All ten pending commits through `f142446` were pushed successfully to canonical
main. Diagnostics are saved outside Git at `logs/restored-access-diagnostics.json`.
Wine 11.18 runtime session `runtime-f9f49bed-d225-4f1c-9073-cd7031a01754`
passed with a fresh Windows execution receipt, exit 0 and no owned processes left.

Three isolated ordinary-Shipping runs completed (session directories under XDG
state `logs/sessions/`, UTC 2026-09-29):

- `17-38-00_3e18f314`: existing msgbox/debugstr trace; exit 1, no explanatory
  message, 309 process snapshots, maximum nine owned processes, no PID 0/1.
- `17-39-23_45189492`: added Wine process/exec channels after inspecting Wine's
  process and shell-execution source; same exit, no game shell handoff captured.
- `17-41-08_8e7cac88`: new opt-in `--engine-stdout` adds only stdout/full-output/
  flush logging arguments found in the supplied executable; no earlier engine
  error appeared. Exit 1, 307 snapshots, maximum nine processes, no PID 0/1.

All three report no EAC/BattlEye process or matching module-load evidence observed,
no Unix crash, no timeout and no owned processes remaining. Anti-cheat absence is
sampled, not absolute attestation. No game files, authorization checks, launcher
stand-ins, network isolation, graphics components or drivers changed. The absence
of a new message does not prove an authentication or graphics root cause.
`--engine-stdout` reports requested logging, not verified logging activation.

Build and all seven release CTest targets pass; 139 core checks now pass with
GPU/namespace cases enabled. All seven ASan/UBSan targets also pass with leak
detection explicitly disabled (`ASAN_OPTIONS=detect_leaks=0`); previous graphics
LeakSanitizer findings are not resolved by this result. Added opt-in argument
regression coverage and updated the CLI help, README and Linux/12.50 evidence.

After interruption, rechecked the worktree and dedicated prefix: zero active
processes, no test/game handles still pending from the completed runs. Next:
read-only startup file-access evidence before another compatibility hypothesis;
do not repeat the identical exit-1 configuration or claim 12.61 compatibility.

### Read-only syscall trace and bundled-Wine comparison

Committed/pushed/installed tracing checkpoint `0477429`. All earlier commands
were terminal before the next experiments, and the 12.50 prefix was inactive.

Provisioned strace 7.2 into project cache only. Download matched the local Arch
sync database SHA-256 and its detached signature verified against the installed
Arch public keyring after dearmoring a copy. No system package or keyring changed.
Initial gpgv calls used an absent key and then an armored file; both failed
verification, and the downloaded binary was not executed until verification passed.

Read-only file/process syscall trace of the managed offline launcher produced
session `2026-09-29_17-46-42_285f972a`: exit 1, no timeout/Unix crash, no observed
anti-cheat, no remaining owned processes. The game's PID 30085 reached successful
Engine/Binaries and ProgramData metadata queries followed by exit_group(1).
No final EACCES/EPERM or pak access was captured for that PID. DLL search ENOENTs
alone do not prove missing dependencies; Wine subsequently loaded the built-ins.
Trace is local `logs/12.50-startup-files-20260929.log` (about 205 MiB, includes
launcher /proc polling). Ptrace changes timing/debugger visibility; matching exit
codes are not proof of an identical internal code path. No assets were changed.

Registered the same installation separately as `12.50-wine-experimental` and
created a dedicated prefix using the **direct Wine 11.0 executable bundled in the
already imported Proton Experimental 20260924**. This is not a Proton-script or
Steam Runtime game-launch test. Prefix log `prefix-d5b212cf-f4a2-4694-9d70-a1421e735654.log`
and runtime session `runtime-682d2576-0c7a-4b6f-ae05-c485fd94d6cf` verify creation,
Windows execution receipt, exit 0 and clean shutdown.

Game session `2026-09-29_17-48-45_40a1ff46` failed with exit 53/c0000135. Unlike
the unknown baseline exit, this has concrete import errors: wined3d needs
libvkd3d-1.dll, libvkd3d-shader-1.dll and libvkd3d-utils-1.dll. Inspected the imported
Proton script's init_wine and default-prefix copying. Setting its WINEDLLPATH alone
in `2026-09-29_17-49-36_76babe40` still failed the same imports. Those DLLs were
absent from the new plain-Wine prefix but present as x64 PE files in Proton's
default prefix template.

Backed up the experimental prefix as
`20260929T175055078-c4e9b257-0a3e-4843-9b27-e559e719e663`, then copied only the
three missing x64 runtime DLLs from that imported template to experimental
prefix system32 with no-overwrite copying. No game-directory or baseline-prefix
DLL was changed. Session `2026-09-29_17-51-10_d18ecc81` now logs successful loads
of all three DLLs, eliminates c0000135, reaches engine shutdown and exits 1.
No anti-cheat observed or owned processes left. This fixes that runtime setup
failure, not the original unknown startup exit; no graphics rendering is verified.

Found and fixed a reproducibility defect exposed by this experiment: copied launch
commands omitted inherited WINEDLLPATH. Commands and JSON now explicitly record
WINEDLLPATH, LD_LIBRARY_PATH and WINEARCH when inherited. No full environment dump
or credential capture. Added quoting regression coverage; release and ASan/UBSan
CTest each pass 7/7 (sanitizer leak detection explicitly disabled). See
[runtime comparison](RUNTIME_COMPARISON.md) for commands, file hashes and limits.
Next: investigate early initialization further or validate the complete Proton
path before making any comparative runtime-support claim. Do not fabricate an
authorization failure or relax the no-bypass boundary to force progress.

### Registry tracing narrows the startup lead

Runtime-path receipt fix and comparison evidence committed/pushed as `d962257`.
Added opt-in Wine `reg` tracing after inspecting Wine's registry implementation.
System-Wine session `2026-09-29_17-54-32_619a143a` captures, on the game main thread,
creation/opening of `\Registry\Machine\Software\Classes`, then an attempt to open
`com.epicgames.launcher` that returns a null handle. The game then tears down
LauncherPlatform/LauncherCheck/Solaris and exits 1. Unlike the earlier teardown
names alone, this is direct evidence of an unsuccessful launcher-discovery lookup.
It is not enough to claim that the lookup causes exit or that authentication/EAC
is required. No fake registry entry, Epic-origin argument or bypass was attempted.

The session retains the network namespace and original game arguments; no
anti-cheat observed, no timeout/Unix crash, and no owned processes left. Log size
about 1.5 MiB, stored privately under the session, not committed. Kept the useful
registry channel in explicit `--trace-startup`, updated help/docs/regression
expectations, and preserved the default non-tracing command. Attempts to consult
additional Epic API documentation returned tool HTTP 404; no claims are based
on those unavailable pages. Wine source and observed local logs support the
narrow finding above. The primary game-startup objective remains unachieved.
Final release rebuild passes all seven CTest targets, formatting/diff checks pass,
and the original 12.50 prefix again reports zero active processes. Ordinary
Shipping SHA-256 still matches the previously recorded value. Next experiment
must distinguish a supported development launch requirement from runtime behavior;
do not interpret a missing launcher key as permission to spoof it.

### Native-managed Proton-script startup comparison

Rechecked clean canonical main at `3bea573`. The previous turn was progress:
verified runtime dependency repair, a captured registry lookup, passing tests and
published commits. Remaining comparison gap was direct bundled Wine versus the
Proton script's complete prefix setup. Inspected installed/imported Proton
init_wine, init_session, setup_prefix and run_proc/run before implementing it.

Added `launch ID --offline-probe --proton IMPORTED_ID`. Extracted shared
`prepareProtonSession` from the tested command runner so both paths validate
runtime/script-version identity, private ownership markers, separate compatibility
data, runtime binding and lifetime-held prefix/runtime locks. Dry planning does
not create compatibility data. The same game validation, profile, unshare,
Vulkan gate, process monitoring, anti-cheat trace check and cleanup remain in use.
No wrapper-only zero exit is presented as gameplay success.

Proton's `PROTON_LOG=1` redirects Wine output to a separate file. For game probes,
`PROTON_LOG=0` plus explicit WINEDEBUG instead keeps real traces in monitored
wine.log; actual runs confirmed module and registry lines are present. DXVK info
and VKD3D warnings are explicitly requested. Inherited Steam/Proton/Wine/media
settings are cleared, a private empty Steam-client directory is used, and no real
account metadata is read. The normal Proton steam.exe compatibility helper was
observed; no Epic/EAC stand-in was created. Host mode rejects the external Steam
runtime-launcher service interface to avoid silently handing execution outside
the managed network namespace. This does not claim Steam Linux Runtime support.

Real command receipts passed in new Proton prefixes:

- Experimental `proton-4a8b7dff-d07f-47f2-ac51-e1c347f8f515`, prefix ID `12.50`.
- Hotfix `proton-0cc0a1ed-f9bc-4550-a538-5281133d4b53`, ID `12.50-proton-hotfix`
  (separate registry alias pointing to the same supplied installation).

Game probes both used `--offline-probe --trace-startup --timeout 45` and ordinary
Shipping's unchanged `-log -d3d11` arguments:

- Experimental session `2026-09-29_18-03-19_8c56af6f`: launcher exit 1, real game
  module load, null Epic launcher-key lookup, game Wine PID 0134 subsequently calls
  NtTerminateProcess with exit 1. 241 process samples, maximum 13 owned processes.
- Hotfix session `2026-09-29_18-05-13_60a3ba6d`: same observed lookup/exit sequence
  on game Wine PID 0138; 242 samples, maximum 13 processes.

Both have no timeout/Unix crash, no invalid PID 0/1 selection, no anti-cheat
process/module observed and no owned processes left. Experimental's DXVK log
explicitly names xalia.exe; it is helper Vulkan evidence, not Fortnite rendering.
Steam-connect warnings from the empty client configuration are not an established
cause. No change made to game assets, launcher registration or authorization.

Added synthetic coverage for non-mutating planning, prefix/runtime lock retention,
cross-prefix lock exclusion, changed-runtime rejection, service-handoff rejection,
sanitized launch environment and anti-cheat-module cleanup. An initial test reused
the intentionally corrupted fixture; corrected it to use the untouched second
import and explicitly test the corrupted import's rejection. Review caught an
ordering regression on namespace-restricted CI: import validation now runs before
the namespace probe, without creating storage. Added a denied-namespace fixture
to preserve that behavior. Release core now passes 153 checks; full seven-target
release tests pass. ASan/UBSan validation is performed with leak detection
explicitly disabled, not as resolution of earlier graphics leak findings.
Final rebuilt ASan/UBSan suite also passes all seven targets with that explicit
setting; formatting and diff checks pass. Both real Proton probes were terminal
with cleanup complete before documentation/commit, not left running in background.

Next: the runtime comparison has not moved past the early launcher-discovery
sequence. Investigate an authorized development-launch contract or obtain stronger
non-mutating causal evidence; do not add identity flags, fake registrations,
authentication replacement modules or repeated identical retries.

Proton host-probe implementation committed/pushed/installed as `2cd2b62`.
[GitHub Linux CI run 36610184260](https://github.com/Zxre0/FortniteOnLinux/actions/runs/36610184260)
completed successfully: dependencies, formatting, compilation and asset-free tests.
The workflow emitted a checkout@v4/Node20 deprecation warning; it did not fail.
No gameplay or asset-based test is included in that CI result.

Successfully located Epic's current Runtime/LauncherCheck API documentation after
the earlier unsuccessful Developer-path queries. It describes launcher-origin
checking and launcher execution responsibilities, but is not Fortnite 12.50 source
or an authorized offline-launch specification. Linked the authoritative reference
in FORTNITE_12_50.md without inferring launch flags, causality or a protection bypass.
The implementation goal remains active and incomplete: real startup stops early
across the measured runtimes, and a legitimate private execution contract has not
been established. UI work or another identical retry would not resolve this gap.

### Development-launch prerequisite audit (2026-09-29)

Repository remained clean at `8f8c403`; the permissions-response turn verified
GitHub connectivity and a successful `git push origin HEAD` reporting everything
up to date. That was a status/access verification, not game-startup progress.
This turn re-read current launcher classification, the real Experimental trace,
the recorded upstream launcher chain, and supplied loose configuration filenames.
The game Wine PID's exit-1 trace remains distinct from Proton/helper exits; no
new root cause was established. Baseline prefix status reports zero active
processes. No matching Fortnite log/CrashContext/UE4Minidump filenames were found
in the supplied game directory or baseline Wine/Experimental Proton prefixes.
Search scope does not justify asserting that all possible diagnostics are absent.

New primary-source evidence rules out assuming that Epic Games Launcher's offline
mode is a supported Fortnite launch option: Epic's 2019 announcement explicitly
excludes Fortnite, and the current Fortnite support answer says no offline mode.
Sources and applicability limits are recorded in FORTNITE_12_50.md. This changes
the next action: do not install/invoke that launcher as a speculative offline
workaround. It does not prove a 12.50 authentication/EAC failure or rule out a
separately authorized development executable and interface.

No justified runtime/game fix follows from the current evidence. Further startup
advancement needs a verifiable, non-bypass development launch contract/build or
new diagnostic evidence that supports a different in-scope compatibility fix.
No new game launch, protection modification, asset write, prefix reset or driver
change was performed. The goal remains incomplete; this is a prerequisite audit,
not a milestone or gameplay claim. Changes are documentation only, so validation
is source/link inspection and Git diff checking, not another game or build test.

### Follow-up prerequisite audit (2026-09-29)

Previous turn added primary-source evidence and pushed `1682bfe`; it did not
advance game startup. Revalidated clean main, current CLI capabilities, supplied
profile, registered build metadata and module validation code. All three build
registrations still reference the same supplied 12.50 installation; aliases are
not additional game builds. The app-owned configuration/data TOML filename scan
found only builds.toml, with runtime/prefix/backup trees excluded. No supplied
development profile or module compatibility manifest was discovered in that
scope. The 12.50 profile still correctly keeps private_path_verified false.

Corrected stale profile recommendation text claiming no game-runtime comparison
had occurred. It now reflects the measured early exits without recommending a
runtime as working. No launch arguments, safety gates or 12.61 profile changed.
This consistency correction does not resolve or reset the same development-launch
prerequisite blocker. No new in-scope launch experiment is justified by this audit.
Further advancement remains dependent on new diagnostic evidence or a verifiable
authorized private-development execution contract/build. No game was relaunched.
The profile resource rebuilt successfully and all seven release CTest targets
passed (14.79 seconds); Git diff checking passed. These asset-free regressions
do not establish game startup or a valid private execution path.

### Blocked audit: development startup remains unresolved (2026-09-29)

Third consecutive prerequisite audit reached the same condition. Previous turn's
profile correction and passing regression tests were verified maintenance, not
advancement past the startup failure. Rechecked local/remote main at `ebfc5f5`,
the current unverified profile, test completion, both terminal Proton result
files and live process/prefix state. Both Proton results still report exit 1,
no Unix crash, no observed anti-cheat and no owned processes remaining. Current
baseline prefix status reports zero active processes; the readable process-name
scan found only this diagnostic CLI among Wine/Fortnite/Proton/Reboot names.
There is no live game probe to wait for or restart based on an expired tool handle.

The objective is not complete. The retail 12.50 install is validated and isolated
startup is reproducible, but the exit's cause and an accepted private-development
execution path remain unverified. Existing evidence does not justify another
runtime change, launcher identity spoof, authentication replacement or protection
patch. Other launcher features cannot substitute for that missing startup result.

Mark the active goal blocked after this audit. Resume startup work when supplied
with a verifiable authorized 12.50 development build/launch specification (including
supported arguments and private backend interface without protection bypass), or
new logs/source evidence supporting a distinct in-scope compatibility experiment.
Additional filesystem permissions and GitHub authentication are not prerequisites;
both are working. No new game test or source change was made in this audit. This
status records an evidence/input impasse, not proof that private operation is
fundamentally impossible or that EAC caused the observed exit.

### User-requested Reboot startup comparison (2026-09-29)

Resumed investigation at the user's request to check how Reboot handles the early
exit. Re-read pinned Reboot Launcher argument generation, game process creation,
helper suspension, authentication-module request and post-login host callback;
also inspected Reboot 3.0 DLL entry/init and build-linux.sh. The server module is
requested after login in this launcher, not as a standalone initial-launch fix.
The helper routine starts processes before calling suspend, so its name alone
does not establish that EAC code never runs. No excluded chain was implemented.
Added source-linked stage analysis without copying tokens or upstream code.

Found an independently testable upstream OPENSSL_ia32cap setting. Read official
OpenSSL 1.1.1 capability documentation and found the 1.1.1 version string in
Shipping. These are a bounded experiment rationale, not proof of a crypto fault
or of environment-variable consumption. In particular, the upstream value lacks
the colon that the documentation uses for extended capability-word control; no
claim that it is a SHA-extension fix was made. Aftermath deletion was rejected
as unsupported by the captured failure evidence, and the file remains intact.

Implemented explicit recording of this already-inherited variable in Wine and
Proton launch receipts, with no default mask. Three additional synthetic checks
verify both plans record the explicit value and default planning does not enable
it. Release rebuild and all seven CTest targets pass (14.91 seconds).

One real isolated Wine 11.18 probe, session `2026-09-29_18-23-55_26904814`, tested
only that upstream environment difference. It still exits 1, records the same
null launcher-key lookup/engine teardown, has no observed anti-cheat evidence,
and leaves zero owned processes. Shipping and Aftermath hashes are unchanged.
The process log contains 305 samples, at most nine owned processes per sample,
and no selected PID 0/1. Release core coverage now reports 156 checks passed.
Full command/environment, Wine trace, process samples and result persist in the
session directory. No private startup, root cause or 12.61 compatibility is
claimed. Do not promote the ineffective setting into the profile or repeat it
unchanged. The no-bypass development-launch prerequisite remains unresolved.

### Authorized local launcher-context experiment (2026-09-29)

The user explicitly broadened local test authorization. Kept real accounts,
production connectivity, anti-cheat process execution and authentication modules
out of the experiment. Implemented an opt-in, 12.50-profile-specific launcher
context argument, gated on explicit --offline-probe and the existing functioning
network namespace. No default arguments or 12.61 behavior were changed.

System Wine 11.18 session `2026-09-29_18-28-07_e5588ee2` ran Shipping with
`-log -d3d11 -epicportal`, OpenSSL mask unset, --trace-startup and a 45-second limit.
This changed the measured outcome: engine/D3D11 initialization, NVIDIA RTX 4070
SUPER selection, game-feature loading and logged manual sign-in UI activation,
instead of the previous early exit 1. Captured newly generated FortniteGame.log
in the session as fortnite.log. No visual/login/gameplay success is inferred.

Observed WineD3D GL_INVALID_ENUM buffer errors, offline patch-check/autologin
failure and a game-internal UAC direct-launch warning. Aftermath loaded without
deleting it. No EAC/BattlEye process/module observed; 786 process samples, max nine
owned processes, no PID 0/1. Supervisor ended the still-running process at timeout
and cleaned up. Raw QProcess crash-exit/exit15 represents that controlled stop;
do not diagnose it as a spontaneous crash.

Five initial experiment-gating/planning tests passed with all seven release CTest
targets (14.82 seconds). Added a further anti-cheat-stop/cleanup regression for
the opt-in path before final retest. Next: same argument via Proton/DXVK to compare
the observed graphics failures. Private login/server compatibility remains open.
Final release retest passes all seven targets (25.13 seconds), including the new
opt-in anti-cheat cleanup check. Shipping/Aftermath hashes still match. The game
log supplies actual engine metadata: 4.25.0-13137020, compatible 4.25.0-13136868;
static build detection still honestly reports unknown where PE metadata lacks it.

### Proton/DXVK visible startup checkpoint (2026-09-29)

Committed and pushed the gated context experiment as `aa761e8`, then performed
the graphics comparison justified by the actual WineD3D errors. Experimental
host-script session `2026-09-29_18-31-49_582d3fb7` used the same context argument,
no OpenSSL mask, baseline SEH/module tracing and a 60-second timeout. It produced
Fortnite-specific DXVK logs identifying v3.1.1-27-g25ca63f17f34bdc and engine-selected
NVIDIA RTX 4070 SUPER. No prior WineD3D GL_INVALID_ENUM/err:d3d matches appeared.

Verified a mapped Hyprland XWayland Fortnite window (PID 63403, steam_app_0,
1920x1080); checked active-window identity/rectangle before capturing only that
rectangle. Visually inspected local fortnite-window.png: the 12.50 sign-in screen
renders with empty credentials fields. Copied this run's steamuser FortniteGame.log
to session fortnite.log. No login attempt, input automation or production network
access was made. No EAC/BattlEye process/module observed; UAC direct-launch warning
remains. Logged handled illegal-instruction exceptions do not establish an
unhandled crash. Audio fallbacks and GfeSDK errors remain unvalidated.

The bounded run ended via supervisor timeout, not spontaneous early exit: 1043
process samples, max 13, no PID 0/1, cleanup complete. This is real rendering/startup
progress beyond the previous blocker, not authentication/private-gameplay success.
Updated README, 12.50 evidence and compatibility matrix to separate baseline,
logged-only Wine startup, visually verified Proton sign-in, and still-untested
12.61/gameplay/audio/input behavior. Screenshot and raw logs stay outside Git.
Next reproducibility work: automatic game-log capture and explicit distinction
between controlled timeout termination and spontaneous runtime crashes.

### Reproducible game-log capture and stop reporting (2026-09-29)

Added a separate bounded game-log collector after completed process cleanup. It
scans at most 64 real prefix-user directories for the exact FortniteGame.log
location, rejects paths outside that prefix and symlinked files, skips logs whose
mtime predates launch, caps reads at 64 MiB, preserves existing session output,
and records per-file status/source/destination/size/mtime. Timestamp freshness is
explicitly a metadata check. No host account directories or arbitrary logs are
searched. New unit cases cover capture, no overwrite, stale logs, symlink rejection
and oversized sparse input. The launcher does not collect while owned processes
remain. Added stop_requested plus a conservative crash_diagnosis so QProcess
CrashExit after cleanup is not silently presented as proof of spontaneous failure.

All seven release CTest targets pass (25.16 seconds); core now reports 172 checks.
The real repeatability probe `2026-09-29_18-38-39_a26403a9` ran the same isolated
Proton/context setup for 35 seconds. Engine again reaches manual sign-in, and the
new collector automatically saves 287004 bytes to session fortnite.log. It reports
controlled termination explicitly: stop_requested true; raw crashed flag retained
as QProcess status, not an unhandled-game-crash claim. 614 process samples, max 13,
no PID 0/1, no observed EAC/BattlEye, cleanup complete. Shipping SHA-256 unchanged.

GitHub CI completed successfully for context implementation `aa761e8`
(run 36612676591) and visible-startup evidence `a91a8b7` (run 36613110424).
These are asset-free CI results, separate from the real game probes. The next
integration boundary is private backend/login/server behavior: the currently
rendered screen is not authenticated or in a lobby, and host/private networking
remains deliberately inaccessible to the diagnostic namespace.

### Private-backend interface discovery (2026-09-29)

Continued the original Linux-first project from clean main `eae2ce3`. Previous
work was concrete progress: verified sign-in rendering, automatic game-log capture,
regression tests and pushed commits. No interrupted game process was assumed live.
GitHub CI for eae2ce3 completed successfully (run 36613755897), separately from
the real game evidence. 12.61 files remain unavailable and claims remain separate.

Read pinned upstream backend bootstrap/helpers, package metadata/license, port
constants, XMPP listener, cloud-storage configuration delivery and game-server
advertisement. Findings: HTTP 3551, XMPP/matchmaker listener 80 versus delivered
client XMPP config 8080, and separate advertised gameplay port 7777. The helper's
/unknown HTTP check proves reachability only. Upstream request-body logging and
implicit listener binding are not adopted. No upstream server/code/assets/keys
were installed, executed or copied. Recorded interface/licensing limits in
REBOOT_LAUNCHER_ANALYSIS.md. Current host loopback UDP 7777 bind probe passed;
corrected stale sandbox-denial wording without claiming firewall reachability.

Read-only supplied-binary strings identified MCPCONFIG= and backend diagnostics.
Implemented an opt-in, profile-defined localhost selection experiment, gated on
existing context and network isolation. Five additional tests cover missing
isolation/context, wrong build and Wine/Proton planning. All seven release CTest
targets passed (25.16 seconds); core now reports 177 checks.

Real Experimental probe `2026-09-29_18-46-46_f124f659` ran for at most 35 seconds.
The engine recorded -MCPCONFIG=localhost but still selected the normal production
Fortnite host with empty override. No working backend redirect was established;
requests remained blocked, and the engine returned to logged manual sign-in.
Automatic game-log capture saved 287026 bytes; 598 process samples, max 13, no
PID 0/1, no EAC/BattlEye process/module observed, and cleanup complete. No raw
logs or credentials are committed. This negative result prevents treating an
unsupported argument as a functioning private-server workflow.

Next: determine the actual supported backend configuration contract, if present,
or a separately reviewed local-only adapter; preserve isolation and the known
sign-in baseline. Do not start the upstream backend unchanged, assume XMPP ports
agree, disable certificate checking, or claim multiplayer from a sign-in screen.

### Built-in Localhost selector: missing service configuration (2026-09-29)

Confirmed b93a6af pushed to main and asset-free CI passed (run 36614815852).
Read-only binary strings explicitly identify epicapp override diagnostics and a
Localhost backend entry. Replaced only the opt-in local-backend experiment's
profile argument with -epicapp=Localhost; baseline/context-only behavior stays
unchanged. All seven release CTest targets pass (25.16 seconds).

Real isolated session 2026-09-29_18-53-34_b7a20b71 recognizes the new selector but
logs missing Fortnite MCPConfig Localhost for service fortnite, host unconfigured,
environment GameDev, override Localhost and invalid-backend rejection. Unlike the
MCPCONFIG experiment, this does alter selection; it still cannot route to a
private backend. No service/authentication/certificate changes were made. The
supervisor ended the runtime at 35 seconds and completed cleanup, with no observed
EAC/BattlEye. Automatic log capture preserved 141314 bytes. The original
context-only Proton sign-in baseline remains available without this opt-in.

Next: surface these exact engine/backend/startup observations in structured
diagnostics, with evidence locations and no false gameplay or crash claim. A
game-side private-service configuration contract remains unresolved.

### Structured game evidence and real-run verification (2026-09-29)

Added a separate bounded analyzer for exact known engine-version, compatible
engine-version, backend-selection, missing-configuration, rejection and manual
sign-in diagnostic lines. Reports include source line numbers and explicit limits;
no rendering, authentication or gameplay is inferred from log markers. Arbitrary
command lines and HTTP payloads are not copied into the summary. Collection adds
the report to each captured log, and a backend rejection is surfaced separately
from the supervisor's raw timeout/signal status. Historical logs can be inspected
with `fortnite-linux logs analyze PATH` without launching the game or network code.

Fifteen asset-free evidence tests cover empty logs, timestamps/BOM/CRLF, engine
metadata separation, missing configuration, sign-in markers, mixed observations,
false-positive substrings, credential-like fields, bounded evidence/lines/input,
CLI errors and automatic capture integration. All eight release CTest targets
pass (25.20 seconds). Historical real sign-in and Localhost rejection logs produce
distinct correct summaries. Updated the stale current-handoff section above;
older baseline failures remain in the historical record rather than being erased.

Real repeat session `2026-09-29_18-59-23_ae62dde0` validates the new automatic
reporting, not another blind attempt at the same backend. It captures 141314 bytes
with all five expected observations, including missing configuration at line1488
and rejection at line1490. The top-level game_reported_blocker is explicit. At the
30-second bound, the supervisor stops and cleans up; 520 samples/max13/no PID0/1,
no EAC/BattlEye observed. Shipping SHA-256 remains unchanged. No private backend
or authentication component was started. The missing configuration contract and
missing 12.61 installation remain the next startup/integration prerequisites.

Rebuilt the current ASan/UBSan configuration and passed all eight CTest targets
(39.46 seconds), with ASAN_OPTIONS=detect_leaks=0 and UBSAN_OPTIONS=halt_on_error=1.
This is not a resolution of historical graphics-stack leak findings. Installed
the current release CLI to ~/.local/bin and verified its historical-log command.
The isolated game-evidence test also passes all 15 checks with leak detection
enabled; it does not initialize the graphics stack, so this does not close the
separate graphics leak issue.

Final configuration lead: read-only strings expose OnlineSubsystemMcp.BaseServiceMcp
and Fortnite.McpConfig, but no accepted schema or override contract was established;
a static reference scan did not resolve usable configuration call sites. No game
binary/configuration/certificate changes were made. This is an unresolved interface
prerequisite, not proof that every private configuration path is impossible.
Do not retest identical missing-config arguments without new evidence. The native
diagnostics implementation and its tests are complete for this checkpoint; the
original private-gameplay milestone is not complete.

### Active goal: verified private-backend login (2026-09-29)

The preceding screenshot request confirmed only the known sign-in rendering, not
a login. The new goal remains login to the private backend, with completion
requiring actual game/backend interaction and authenticated private session
evidence. Worktree was clean at 8e4b66f; no project backend implementation or local
listener was present. Rechecked upstream's separate backend/auth-module interface.

Found new external evidence beyond the earlier string-only lead: an author-published
UEFN INI example uses Protocol and Domain under the matching BaseServiceMcp section,
and current Epic documentation describes command-line INI overrides. This does not
establish 4.25 shipping support, but supplies a specific isolated experiment. Added
a typed BackendProbe selection and a separate service-ini method, limited to the
12.50 profile and loopback address. No authentication, certificate, game-file or
production-network changes. Added version/isolation and Wine/Proton planning tests.
Source links and scope are recorded in NETWORKING.md. All eight release CTest
targets pass (25.18 seconds); the actual routing experiment follows the build.

Command-line service-ini run 2026-09-29_19-13-02_9401e23d retained the production
host and manual sign-in markers; the only loopback address mention was its
recorded argument. 35-second timeout, 614 samples/max13/no PID0/1, no observed
EAC/BattlEye, cleanup complete; captured game log 287114 bytes. bVerifyPeer remains
true. Added two CLI option-rejection checks; final release suite passes 8/8 in
25.23 seconds (183 core checks). No backend was started or credentials supplied.

Tested file delivery separately rather than repeating the same command. The game
log identifies its prefix-local WindowsClient/Engine.ini path. Confirmed it was
absent and its parent canonicalized within the owned Proton prefix; created only
the two settings from profiles/experiments/service-address.ini. Isolated context
run 2026-09-29_19-14-23_242fb8aa again retained the production host/manual sign-in,
with 286900-byte game log, 607 samples/max13/no PID0/1, no observed anti-cheat and
completed cleanup. Compared the unchanged test INI against the source template
and moved it into that session as tested-Engine.ini, restoring the original
absence of Engine.ini. Both copies hash to
ed288c8c602b3546d92e5647545f67add81144e722d50ae5b5018d28a49c8add.
No game installation files or user configuration were overwritten.

New source lead changes the next action: independently found public Sinum source
19fbed919824d1cfba8228158feec28f2fa12403. Read the license, initialization, routing,
Unreal string/allocator boundary and build target. URL routing is structurally
separate from the complete module's exit/security patches. The latter are not
adopted, and no upstream binary is run. Notes record the custom license restriction,
unverified 12.50 ABI/addresses and missing checked Windows development headers.
Next work toward the actual login goal: review a strictly local, routing-only
adapter and its required ABI/toolchain, then prove requests reach an isolated
backend before testing a development account. Do not claim the login goal achieved
from these negative configuration experiments or the existing sign-in screenshot.

### Read-only HTTP interface candidate established (2026-09-29)

Current milestone remains actual private-backend login, not the existing sign-in
screen. Previous screenshot-only goal turn made no implementation progress; resumed
by validating the unfinished helper against authoritative logs and source.

Added an original, optional Windows PE helper built by local Clang 22.1.8/LLD with
Wine's existing headers/kernel32 import library. Native launcher remains Qt/C++ ELF.
No upstream code or security/exit patches are copied. The helper creates only its
own ordinary Shipping child (or a synthetic fixture), assigns a kill-on-close job,
then reads that child's image. It reports a stat-marker reference, PE exception
function bounds and pointer-reference RVAs; no arbitrary PID attachment, remote
writes, injection, authentication, certificate changes or routes are implemented.
12.50-only launch gating preserves network isolation, scoped supervision and
anti-cheat observation. The 12.61 profile remains untouched/unverified.

Build investigations: Wine MSVCRT headers satisfy the Windows-header dependencies;
a local freestanding memset avoids CRT linkage. Header ordering needs windows.h
before tlhelp32.h. Fixture unwind metadata was absent until -funwind-tables was
enabled; no bounds-validation relaxation was used. A large automatic report-path
buffer initially required unavailable ___chkstk_ms, fixed by static storage.
Reproducible linker timestamps use /timestamp:0. Initial quoted/unquoted target
handling and fixture prefix-ownership setup were corrected and retested. Fixture
paths with spaces passed in http-fixture-4238f020-2949-428f-8510-2b95198681d7.

First real probe 2026-09-29_19-31-45_f7970472 loaded both helper and Shipping under
Proton Experimental 11.0-20260924 but yielded no stdout report, despite wrapper
exit zero. This was insufficient evidence of helper success. Its reliance on the
console handle was replaced with a CREATE_NEW session report, an explicit completion
record and a bounded session-checking native parser. Wrapper exit is now labeled
as wrapper scope, and missing/failed receipts produce a launcher error. Session
directories are owner-only. No backend was started during these probes.

New asset-free Wine fixture http-fixture-f4a11c05-9917-4b6e-a041-0ef0221910bb passes
with the report-file path, exact fixture candidate and scoped cleanup. The native
release suite passes all eight CTest targets (25.21 seconds), including seven new
receipt cases for valid, missing, mismatched, truncated, duplicate, oversized and
completed-without-candidate outcomes (197 core checks in total).

Real command: env -u OPENSSL_ia32cap ./build/fortnite-linux launch 12.50
--offline-probe --launcher-context-probe --proton experimental-20260924 --timeout 60
--http-probe-helper build/windows/fol-http-probe.exe.
Session 2026-09-29_19-39-02_27490309 produced a complete matching receipt. First two
scans found the marker but no executable reference; the third found exactly one:
xref RVA59236044, function [59236016,59236476), pointer RVA107847656. Helper SHA256
74fdd76e723896345f822fc2f5d112160e86db138f7f0a48446681ecdced97ef.
The Windows child PID was336. Native supervision reports no remaining owned
processes and no observed EAC/BattlEye. The helper deliberately ends its child after
discovery, so the older engine log was correctly rejected as stale. This is not a
game crash, a backend request, authenticated login or established callable ABI.

Next: validate whether the pointer belongs to the expected request interface and
establish the 12.50 method/string ownership contract before a local-only adapter.
Static bytes at the runtime function RVA do not decode as that runtime function;
do not infer ABI from those on-disk bytes or hardcode nearby-version hook guesses.

### Pointer-table boundaries and URL-method hypothesis (2026-09-29)

Committed/pushed d5c0f04, and GitHub Linux build36620990205 passed. Continued the
same login goal with read-only pointer-table observations rather than adopting
nearby-version offsets. The helper now checks every byte in observed spans and
bounds table scans to64 slots on either side. Synthetic fixture now has two
distinct functions; manual tests verify exactly two table entries and index0 for
its request function (http-fixture-70d2c185-d7d6-4870-b2c2-5fb478b56e52 and
http-fixture-183e3e0d-8b0b-4c89-b4cb-b141afd16650, both passed/cleaned up).

Real probe 2026-09-29_19-43-42_ef7be1bd found65 contiguous function pointers at
RVA107847440 with the marker function at index27. 192 snapshots/max15/no PID0/1,
no observed anti-cheat, cleanup complete. Shipping SHA256 remains
34a10af4ddff20b75d889e269a66644d570c3000f13548a5799d3a7ef690190c.
Follow-up 2026-09-29_19-44-51_59da246c additionally checked references to interior
table addresses, finding indices0,5,41,51,58. A contiguous run is not a vtable
boundary, so none of those findings authorize invoking arbitrary entries.

Added explicit FOL_HTTP_PROBE_CODE=1 diagnostic, limited to128 bytes at each
observed executable function (max129); default does not record code. It is logged
in the launch command and never reads request objects or credentials. Code remains
in owner-only local session artifacts, excluded from summary JSON and Git.
Asset-free fixture http-fixture-298766b0-1c9d-4892-9bb7-0fcf056f8382 reported two
code observations and passed. Real session 2026-09-29_19-46-43_f25f2108 added65
bounded observations with complete receipt and scoped cleanup. Inspected these
with local llvm-mc using the x86_64 Windows target; truncated/padding disassembly
warnings were not treated as valid method instructions.

Method evidence now supports a logical interface beginning at run index10:
getter copies object offset104, setter ten logical slots later assigns offset104
after a request-state check, and another getter copies offset120. The marker
function would be logical slot17. Full reasoning/limitations are in
REBOOT_LAUNCHER_ANALYSIS.md. No function invocation, heap/request scan, remote write,
module injection, routing change or backend service was attempted. Current next
task is a controlled request-interface validation and local-only routing design,
followed by actual backend requests; login remains unachieved.

Final release regression run passes 8/8 CTest targets in 25.17 seconds (201 core
checks), including bounded table/code receipt validation and non-propagation of
code samples into the launcher summary. The real code-observation helper hashes
to 99f6a063221e4302abcc8d7a15bb716d5809a65a5971df6461fbfd44ae5bd60f.

### Actual-request observation and upstream lifecycle comparison (2026-09-29)

Previous goal turn made progress: two pushed commits and real interface evidence;
a2cef6c GitHub Linux build36622020526 also passed. Goal remains actual private login.
Implemented a separate optional hardware-breakpoint observer to validate the
candidate against a live request, without changing request/code/authentication
behavior. Hash-gated to the exact supplied 12.50 executable; layout lives in the
12.50 profile. Debugger attaches only to its own child. Cleanup checks thread process
ownership, skips exited threads, restores debug settings, then detaches. No arbitrary
PID argument, module injection, game memory write or security/exit patch is added.

Synthetic request fixture now repeats calls with a known string descriptor at
offset104 and a fake sensitive query. The observer caught eight calls, reported
only loopback/oauth-token categories and omitted the query secret. Sessions
http-fixture-64ff0aa5-9bab-498b-af1f-0272f61b489f and
http-fixture-0c13ced5-d957-4b9e-a051-6b3c24819976 passed with scoped cleanup.
Release regression suite initially passed8/8 in25.33s (207 core checks).

Real command used the existing isolated Proton context launch with
--http-probe-helper build/windows/fol-http-observer.exe. Results:

- 2026-09-29_19-59-24_50fe0046: attachment immediately after discovery; no request
  caught. Access violation followed by child exit0xc0000409; no fresh engine log.
  230 samples/max15/no PID0/1. Helper receipt correctly reports failed observation.
- 2026-09-29_20-01-28_f07bfb8b: explicit10000ms attachment delay. Engine initialized,
  but game exited3 after an execute access violation at0x1812ccc60 (unknown module
  in Wine trace);159653 bytes of engine log captured, no request observation.
- 2026-09-29_20-03-09_40e9e9f9: hold-only control, no debugger attachment. Child stayed
  alive for the20s control period and logged manual sign-in;286990-byte game log.
  This establishes that the helper/child launch chain can preserve that baseline.
- 2026-09-29_20-04-38_cd21790c: attach-only control with10000ms delay; no hardware
  breakpoint installed. Child again exited3;235102-byte game log, no request.

Every real run above completed owned-process cleanup with no observed EAC/BattlEye.
No listener, credential submission, URL change or login occurred. The failure is
associated with debugger attachment in this configuration, not proof of a specific
protection mechanism. Normal baseline also has exception-driven startup activity;
unrelated exceptions were not swallowed and no exit behavior was patched.

User reminded us to consult Project Reboot. Re-read Reboot Launcher's game creation,
auth injection, _onLoggedIn and LoadLibrary/remote-thread utility, plus Reboot3.0
DllMain and Sinum's separate initialization/routing source. Upstream loads a
Windows module, not a debugger, before login and loads the game-server module
after login. Existing suspended EAC helpers and unrelated Sinum exit/security
patches are not appropriate here and remain excluded. Next: owned-child module
lifecycle, tested first with original asset-free fixtures, then narrowly scoped
request observation/local routing. Do not keep retrying the proven failing
debugger configurations or claim the overall login goal blocked/completed.

Final native regression run passes 8/8 targets in 25.22 seconds, including
observation/control distinction, redacted category validation and executable-hash
rejection. Attach-only control has 403 process samples, max16, no PID0/1 selected.
The original read-only discovery helper remains available separately. No Fortnite
files, prefixes outside the project, certificates or system packages were modified.

### Owned-child module lifecycle verified (2026-09-29)

Following the upstream architecture comparison, implemented an original Windows
bootstrap and readiness-only DLL. This is a separate opt-in diagnostic, not a
replacement for the native Linux launcher. Only the bootstrap's freshly created
child may receive the module. It copies a pathname to fresh RW memory, resolves
the child's LoadLibraryW including forwarded exports, waits for completion and
verifies module presence. No debugger attachment, instruction/security/exit patch,
request hook, credential submission or networking is involved. Module worker
receipt is independently checked against the session and child PID. Native launch
checks the exact supplied Shipping SHA256, x64 DLL format, version, role and hash;
the build-generated manifest alone is not runtime compatibility certification.

Built with Clang22.1.8/LLD and existing Wine headers/import libraries. Asset-free
fixture http-fixture-beeb5b73-6597-4c3f-98f2-bf8d1f5d0023 passed: module loaded,
receipt from child212, child alive20s, scoped cleanup complete. Real command:

```sh
env -u OPENSSL_ia32cap ./build/fortnite-linux launch 12.50 \
  --offline-probe --launcher-context-probe --proton experimental-20260924 \
  --timeout 60 --http-probe-helper build/windows/fol-module-bootstrap.exe \
  --http-module build/windows/fol-http-adapter.dll
```

Real session2026-09-29_20-14-37_f7e318b5 succeeded at module lifecycle only:
module presence and ready receipt from child336, child alive20s, helper exit0,
owned_processes_remaining=false. DLL SHA256:
a3235088356da406af0a490d7dcc91def36610713ca7d547bd980bfa81428f34.
Fresh Fortnite log286988 bytes retains manual sign-in and production backend
selection (unreachable in the isolated namespace). No observed EAC/BattlEye.
No screenshot was captured for this run; no private backend was started or reached.
No login or gameplay claim is warranted. Unlike the debugger approach, ordinary
module loading preserved the established sign-in baseline. Next: narrowly scoped
in-process request observation before implementing any local routing/backend.

Native regression:8/8 CTest targets pass in25.27s, including219 core checks for
receipt identity/truncation, explicit version matching and bootstrap requirements.
All original work remains in FortniteOnLinux; upstream source was read, not copied.

### In-process request observer experiment (2026-09-29, ongoing)

Checkpoint3329aa4 is pushed and its GitHub Linux build passed (as did0d8c354).
Added a separate original request-observer DLL, not a change to the lifecycle-only
artifact. The native launcher requires the distinct manifest role and exact12.50
Shipping hash, supplies the checked layout and validates a separate observation
receipt. The module changes only one validated HTTP data pointer, forwards every
call unchanged, then restores the pointer/page flags. It does not change URLs,
authentication, certificate verification, game code, exit behavior or game files.
No backend listener has been started; no credentials have been submitted.

Fixture9207fa25-844e-4688-89bd-60458ec8fc34 rejected layout before installation.
More precise diagnostics in fixture49f44ccd-3eee-4f78-ad71-abbd86941841 identified
table page protection8 (PAGE_WRITECOPY), code32 (PAGE_EXECUTE_READ). Supporting
that ordinary data mapping produced a passing fixture
http-fixture-8d372849-3ebd-4545-a9cc-21ae64ce37cf: eight loopback/oauth-token
categories, no fake query secret, original return preserved, pointer restored,
child remained alive, cleanup complete. Earlier failures retained local receipts;
the runner now also writes a result.json on failed observations.

First real observer session2026-09-29_20-30-15_4e0da321 safely rejected before
installation: table and code protections128 (PAGE_EXECUTE_WRITECOPY). It still
logged manual sign-in, no observed EAC/BattlEye, no remaining owned processes.
llvm-readobj confirms tableRVA107847656 is in non-executable .rdata in the checked
Shipping file, while functionRVA59236016 is in .text. Added separate runtime PE
section-role checks, and preserve existing execute permission on mapped data
pages during pointer replacement/restoration. This addresses measured mapping
semantics; it is not a security-check patch. Retesting fixture before another game
run. Latest completed native regression:8/8 in25.50s with229 core checks.

Section-aware fixture http-fixture-ebbce692-4237-4a43-825d-30987c8d5f78 passes
all eight observations/restoration/return-value checks with scoped cleanup.
Real retest2026-09-29_20-33-09_cdc67e77 succeeds at actual request observation:
seven matching request objects, zero rejected layouts, two epic/other and five
epic/oauth-token categories. Method pointer installed and restored; module receipt
from child336; child remained alive25s; helper exit0 and cleanup complete. Original
request calls/results are forwarded; no URL rewrite occurred. Manual sign-in is
still logged in286972-byte Fortnite log. No EAC/BattlEye observed during sampling.
DLL SHA256d216246dbee4ef399d080553e0ec7ab450d1aab353b52af9799f02347dd87ab1.
The automatic token-endpoint calls are not a successful authentication, credential
submission by this project, or proof of backend reachability: networking remained
isolated and no backend listener exists yet. No request payload or raw URL was
logged. This verifies the real vptr/offset/method hypothesis that was previously
only inferred from code. Next: independently implemented strict loopback routing,
then a same-namespace private service with explicit development authentication.

Final regression after the section/protection fix:8/8 CTest targets in25.25s,
229 core checks. The separate lifecycle-only DLL still passes with the updated
fixture in http-fixture-b8ba6f83-4a43-4355-9495-3c0b8fbd8a85. No game assets,
raw code observations, DLL artifacts, private prefixes or credentials enter Git.

### Local routing and native backend transport (2026-09-29)

The user changed the active objective to making a backend this old Fortnite can
log into. This remains unachieved until actual player authentication is verified.
Prior goal turn made concrete progress: real interface observations and pushed
c9d19ca; its GitHub build passed. No blocked/complete status is warranted.

Added original strict URL selection (25 asset-free checks), separate router DLL
and profile-bound setterRVA59245424/slot10. Fixture
http-fixture-fdb6d08e-a735-4b57-8ec8-7504243daa34 verifies a copied URL rewrite,
no fake query leak, forwarded method result and pointer restoration. Real session
2026-09-29_21-08-23_39d66d3d verifies three rewrites and game log attempts to
http://127.0.0.1:3551/account/api/oauth/token. Those fail with no listener. No
authentication/certificate/security/exit patches; bVerifyPeer=true. Router SHA256
ea18c64e2a6cf5c64f60cb87dbeb46b233aedb3a3443bcaacb2f5c4b383b86f5.
Cleanup complete, no observed anti-cheat. Native tests passed9/9 in25.17s.

Implemented native backend identity/authentication core, bounded loopback HTTP
server and separate fortnite-backend CLI. Public application tokens carry no player
identity; player tokens require the generated @localhost account's actual local
password. Salted PBKDF2, expiring random access tokens, refresh rotation, revocation
and private account authorization are tested. Init created owner-only identity and
local-login files in project XDG config, not Git. They contain no Epic credentials.
Read upstream user.js/version.js only for protocol shapes; no code, hardcoded
tokens, permissive passwords or request-body logging copied.

Build failures were concrete and corrected: current Qt deprecated the explicit
Qt::UTC overload (use epoch conversion then toUTC); single-name command options
had ambiguous brace initialization; HTTP test needed QNetworkProxy include.
Authentication checks20 passed; HTTP socket checks11 passed including rejection
of duplicate Content-Length and log non-disclosure. Full suite11/11 in25.33s.

Initial namespace smoke failed before interface changes because stat of
/proc/1/ns/net is Permission denied inside unprivileged user namespace. Reproduced
with stat. Replaced that check with a pre-unshare namespace receipt captured by
the trusted native launcher; requires a different current namespace. Isolated
curl /health test then passes, and same-namespace negative test refuses changes.
Only isolated lo is enabled; no host network/router/firewall changes.

First actual combined run2026-09-29_21-19-48_9fe97491 (same command as router test,
plus --private-backend) receives the game's application-token POST and returns200.
Game log records MCP service Normal -> Connected. A subsequent request returns404
and GetUserPrivilege/patch check fails; the game reaches manual sign-in, not player
login. Four requests observed, two rewritten, pointer restored, helper exit0,
cleanup complete, no EAC/BattlEye observed. This is verified game-to-private-service
transport/application bootstrap, not an authenticated player session.

Next implementation based on that response and upstream version route contract:
local versioncheck NO_UPDATE policy and HTTP Date header (game warned it was
missing). Tests/build in progress. Still needed: verify that contract with the
actual client, submit only generated local development credentials, extend routing
beyond its bounded eight-call/20s diagnostic window, and implement subsequent
login/profile requirements from evidence. No screenshot of a successful private
player login exists yet.

Retest2026-09-29_21-24-05_d62d90e5 verifies application-token200 and
local-versioncheck-no-update200 from the actual game. The prior patch-check
failure is gone; a subsequent Fortnite-service404 now produces
UpdateFailure_HotfixCheck. No player grant was submitted. Five request categories,
three rewrites, restoration and scoped cleanup verified; no anti-cheat observed.
Next contract is hotfix/cloud storage, then actual local player authentication.
Final regression11/11 in25.40s:231 core checks,21 backend auth checks,11 HTTP checks,
25 URL-selection checks plus the existing test targets. Namespace-negative tests
prove same/malformed receipts reject before interface changes. No successful
private player-login screenshot or gameplay is claimed.
