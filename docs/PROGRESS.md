# Engineering progress

## Current handoff state

Fortnite 12.61 files remain unavailable. The user supplied a separate 12.50 build;
its metadata, structure and all 398 local-manifest file hashes are verified, and isolated startup diagnostics are in
progress, now blocked by sandbox GPU/socket restrictions. The corrected Wine run
exited 1; its cause is unresolved. No legitimate non-EAC private execution path is
established. Native CLI, synthetic tests, diagnostics
and Wine/Proton command execution work within the measured limits below; no game
startup or gameplay is claimed. Latest pushed code checkpoint: `1e6ba64`.
Last unrestricted release tests passed 109 assertions; sanitized non-GPU tests
passed 100. Socket-independent tests currently pass 107 core and 32 manifest
assertions, including ASan/UBSan with leak detection explicitly disabled. The
separate network test fails UDP bind; do not report the full suite as passing.
Graphics-enabled LeakSanitizer remains unresolved, and sandbox leak checks fail.

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

Resume by inspecting/validating supplied files, registering the detected build,
checking the embedded profile and bootstrap inventory, then running the offline
probe and capturing the first real failure. Do not start authentication modules,
paused EAC stand-ins, protected services, or security-bypass experiments. Remaining
graphics-stack LeakSanitizer findings are documented below, not suppressed.

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
