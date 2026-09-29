# Engineering progress

## Current handoff state

Milestone 1 remains blocked on accessible Fortnite 12.61 files and an established
legitimate non-EAC private execution path. Native CLI, synthetic tests, diagnostics
and Wine command execution work within the measured limits below; no game startup
or gameplay is claimed. Four code checkpoints have been pushed to canonical main
and all four GitHub Actions runs passed. Latest code checkpoint: `e37c201`.

Installed native CLI: `/home/atlas/.local/bin/fortnite-linux`. Source checkout:
`/home/atlas/Projects/FortniteOnLinux`. Dedicated prefixes `12.61` and
`runtime-smoke` are initialized, with no owned processes remaining. Game files,
unrelated prefixes, GPU drivers and boot configuration have not been modified.

Next required external input: an accessible complete 12.61 installation path.
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
