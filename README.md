# FortniteOnLinux

Native Linux CLI/core for investigating Fortnite **12.61** with legitimately
obtained files and authorized local/private development infrastructure.
Canonical repository: <https://github.com/Zxre0/FortniteOnLinux>.

**Current status: experimental foundation; functional Fortnite startup, Project Reboot
integration and gameplay have not been verified.** No complete 12.61 installation
was found on the development machine. A legitimate non-EAC private execution
path has not been established. This project does not bypass authentication,
DRM, SSL pinning or anti-cheat, and distributes no Fortnite assets.

A supplied **12.50** build is now registered as a separate diagnostic-only target.
It does not establish 12.61 compatibility. See [12.50 evidence](docs/FORTNITE_12_50.md).
An opt-in, network-isolated launcher-context experiment now reaches engine
initialization and the logged sign-in UI stage. This is not authenticated login,
private gameplay, visual verification or an anti-cheat bypass.

## Build

Requires C++20, CMake >=3.22, Ninja, Qt >=6.4 Core/Network, toml++ >=3.3,
zlib, and Vulkan headers/loader development files. No Wine is needed to run the CLI
or asset-free tests. On Arch/CachyOS developer packages are `base-devel cmake
ninja qt6-base tomlplusplus zlib vulkan-headers vulkan-icd-loader`.

```sh
./build.sh
./build/fortnite-linux --help
./build/fortnite-linux diagnose
```

Optional user-local installation: `cmake --install build --prefix "$HOME/.local"`.
Ensure `$HOME/.local/bin` is on PATH, or invoke the installed binary by full path.

If using a separately provisioned Khronos header checkout, configure once with
`cmake -S . -B build -G Ninja -DVulkan_INCLUDE_DIR=/absolute/path/to/Vulkan-Headers/include`.
`./build.sh` preserves that CMake cache setting. See [Linux notes](docs/LINUX.md).

## Current commands

```sh
fortnite-linux discover /mnt/games
fortnite-linux add /path/to/Fortnite
fortnite-linux builds
fortnite-linux verify 12.61
fortnite-linux verify 12.61 --manifest /path/to/integrity.toml
fortnite-linux manifest /path/to/build.manifest --installation /path/to/Fortnite --hash-files
fortnite-linux imports /path/to/FortniteClient-Win64-Shipping.exe --dll-dir /path/to/prefix/drive_c/windows/system32
fortnite-linux remove 12.61              # registration only; preserves files
fortnite-linux runtime list
fortnite-linux runtime import-proton '/path/to/Proton - Experimental' --id experimental
fortnite-linux runtime test-proton proton-smoke --proton experimental
fortnite-linux runtime test 12.61        # after creating the prefix; no Fortnite execution
fortnite-linux prefix create 12.61
fortnite-linux prefix status 12.61
fortnite-linux prefix stop 12.61         # only processes in this owned prefix
fortnite-linux prefix backup 12.61       # copy; leaves active prefix in place
fortnite-linux prefix backups 12.61      # list recovery tokens
fortnite-linux prefix archive 12.61      # move aside without deleting files
fortnite-linux prefix restore 12.61 BACKUP_TOKEN  # requires absent active prefix
fortnite-linux prefix reset 12.61        # archive old prefix, then initialize new Wine prefix
fortnite-linux diagnose --output system.json
fortnite-linux launch 12.61 --offline-probe --print-command
fortnite-linux launch 12.61 --offline-probe --timeout 60
fortnite-linux launch 12.50 --offline-probe --trace-startup --timeout 35
fortnite-linux launch 12.50 --offline-probe --trace-startup --engine-stdout --timeout 35
fortnite-linux launch 12.50 --offline-probe --proton experimental --trace-startup --timeout 45
fortnite-linux launch 12.50 --offline-probe --launcher-context-probe --trace-startup --timeout 45
fortnite-linux module /path/to/module.dll --manifest /path/to/module.toml
fortnite-linux network 127.0.0.1 7777
fortnite-linux processes
fortnite-linux logs
```

Registration supports multiple IDs (`add PATH --id custom-name`). Detection reads
ASCII/UTF-16 release metadata and checks PE architecture, section bounds, content
folders and readable nonempty paks. It does **not** prove asset authenticity or
full integrity. Conflicting/unknown metadata is rejected. No unofficial downloads.

Optional integrity manifests contain a `[files]` table mapping quoted relative
file paths to SHA-256 strings. Only listed files are checked; manifest provenance
is not authenticated. Paths escaping the installation, including symlink escapes,
are rejected. The same `--manifest` option on `module` expects `fortnite_version`
and `sha256` fields instead. See [12.61 validation](docs/FORTNITE_12_61.md).

`manifest PATH` independently reads supported legacy binary Epic manifests.
Add `--installation ROOT` to check listed file presence/sizes, and `--hash-files`
to stream SHA-1 verification (a full installation can take several minutes).
This is read-only and never executes the manifest's launcher or downloads files.
Matches prove consistency with the supplied local manifest, not authenticity.
See [manifest verification](docs/MANIFESTS.md) for scope and format limits.

`imports PE_PATH` inventories immediate/delayed DLL names and candidate files in
the executable directory and repeated `--dll-dir` directories. It does not load
DLLs, change Wine search paths, or equate missing optional imports with failure.
See [dependency diagnostics](docs/DEPENDENCIES.md).

Normal `launch` and `host` currently report a blocker. `--offline-probe` is only a
network-isolated startup experiment using installed Wine or an imported Proton script, not working
offline gameplay. It requires unprivileged Linux user/network namespaces and an
initialized dedicated Wine prefix, or imported Proton with separate managed
compatibility data. It never starts the EAC bootstrap intentionally.
Process-name monitoring is sampled; absence from snapshots is not proof that EAC
never loaded. See [EAC findings](docs/EAC.md).

`--trace-startup` adds Wine message-box, debug-string, process, shell and registry
traces in the dedicated prefix. Treat trace logs as private diagnostic material.
`--engine-stdout` separately requests engine stdout logging and line flushing;
Shipping builds may ignore these arguments. They did not explain the tested
12.50 exit 1. Neither option changes authentication or anti-cheat requirements.
Commands explicitly record inherited WINEDLLPATH, LD_LIBRARY_PATH and WINEARCH
when set, without dumping the entire environment. See the
[12.50 runtime comparison](docs/RUNTIME_COMPARISON.md) for the separate bundled-Wine
experiment and its verified dependency fix, which did not resolve exit 1.
The latest registry trace shows a failed Epic launcher-registration lookup before
shutdown; this is a lead, not proof of an authentication or anti-cheat requirement.

The core detects Steam-local Proton installations and tests imported runtime copies.
`launch --proton ID --offline-probe` runs an imported Proton script on the host;
it is not a Steam Linux Runtime/UMU container launch. Experimental and Hotfix
both started supplied 12.50 Shipping and reproduced exit 1. DXVK/VKD3D game rendering validation,
runtime downloads, complete prefix lifecycle,
private backend/server integration, server browser, dev-loop and GUI remain
unimplemented. WineD3D is the unchanged Wine baseline. Gamescope is optional and
currently only detected. No production services or router configuration are used.

Prefix recovery uses private XDG data storage in `prefix-backups/ID/TOKEN/`.
Copy backups preserve symlinks without following Wine's links into the host.
Archive/reset retain the previous prefix; restore consumes the selected backup
by renaming it into place and refuses to overwrite an existing prefix. Incomplete
backups are retained for inspection but cannot be restored automatically. Stop all
prefix users first; launcher locks cannot prevent an independently started Wine.
No permanent prefix deletion or automatic backup pruning is implemented.

`runtime test ID` runs Wine's built-in cmd.exe version command and writes a unique
execution receipt in an offline namespace, then waits for that namespace's Wine
server. This tests Windows command execution
and clean shutdown only; it does not test D3D rendering or Fortnite compatibility.

`runtime import-proton DIRECTORY --id NAME` copies a trusted, already-installed
Proton runtime into project XDG data. It does not download runtimes or modify the
source. `runtime test-proton PREFIX_ID --proton NAME` uses separate
`proton-prefixes/PREFIX_ID/pfx` compatibility data, no game files and no Steam
account. A unique Windows-written receipt and clean server shutdown are required
for success; exit zero alone is insufficient. Custom `user_settings.py` is rejected,
and inherited Steam/Proton/Wine overrides are removed from this baseline test.
This supports the tested `files/bin/wine` layout, not every Proton/UMU release.
Wine prefix backup commands do not currently manage Proton compatibility data.
Proton game probes use the same ownership checks and hold prefix/runtime locks
through execution. A prefix cannot silently switch Proton IDs. Existing Steam,
Proton, Wine and media override variables are cleared and recorded as unsets in
the command; no real Steam installation or account configuration is used.
`--print-command` does not create Proton compatibility data. A discovered
`steam-runtime-launcher-interface-0` is rejected for these host probes because
handing execution to an external service would invalidate the namespace assumption.
The script's `PROTON_LOG=0` keeps its separate log redirection off, while explicit
`WINEDEBUG` enables module/registry traces in supervised `wine.log`. Thus this
setting does **not** disable Wine diagnostics. Runtime-test logging is unchanged.
The reported exit code is the Proton launcher status; logs must establish the
game's own execution/exit. A zero wrapper exit is not gameplay success.
Both runtime tests handle SIGINT/SIGTERM and bounded timeout with session-scoped
cleanup and process-tree logs. EAC-name observation fails the test and stops its
processes. SIGKILL or machine power loss cannot be handled by userspace cleanup.

Configuration, data, cache and logs follow XDG conventions under
`fortnite-on-linux`. Every launch request records a unique session under
`~/.local/state/fortnite-on-linux/logs/sessions/`, including blocked preflight.
The CLI prints JSON and returns nonzero on validation or launch failure.

See [progress](docs/PROGRESS.md), [architecture](docs/ARCHITECTURE.md),
[12.61 research](docs/REBOOT_12_61.md), [compatibility matrix](docs/TEST_MATRIX.md),
and [troubleshooting](docs/TROUBLESHOOTING.md).
