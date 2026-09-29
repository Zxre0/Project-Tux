# Architecture decision: native Linux core first

2026-09-29. Priority: Fortnite 12.61 startup evidence on Linux. No gameplay success claimed.

| Option | Linux/process support | Packaging and maintenance | Decision |
| --- | --- | --- | --- |
| C++20 + Qt 6 | Native ELF; QProcess, filesystem, event loop, Qt Widgets later | Distribution packages; compiler, Qt and toml++ already installed here | Selected |
| Rust + native GUI | Strong process ownership and error types | Good CLI; GUI toolkit choice and new toolchain needed | Viable alternative, no present advantage justifying two stacks |
| Rust + Tauri | Strong core; web frontend | Additional webview dependencies and UI build system | Defer; UI is not first milestone |
| Flutter | Native Linux UI possible | Upstream shared code uses Win32/COM/FFI, requiring process-layer replacement | Learn separation, do not port Windows assumptions |

`fol_core` is a reusable native library. `cli/main.cpp` dispatches commands only.
`src/fortnite.cpp` owns PE inspection, metadata detection and content structure;
`core.cpp` owns XDG paths, atomic TOML registration and embedded compatibility profiles;
`runtime.cpp` owns Wine discovery and dedicated prefixes; `platform_linux.cpp`
owns /proc inspection, pidfd signalling and native socket capability probes. `diagnostics.cpp` owns Vulkan and
network checks. `launcher.cpp` orchestrates session planning and monitoring.
`reboot.cpp` validates supplied modules; it does not inject or patch anything.
`manifest.cpp` independently reads a bounded legacy Epic manifest subset and
optionally verifies existing local files. It never executes manifest commands,
downloads files or treats a local manifest as authenticated provenance.
`pe_imports.cpp` reads bounded x64 import descriptors and inventories explicit DLL
directories; it does not load libraries or emulate the Windows loader.
`prefix_backup.cpp` owns recoverable Wine-prefix snapshots and restoration.
`proton.cpp` owns runtime import and isolated command smoke tests. Proton data is
separate from Wine prefixes; neither manager silently adopts the other's storage.
Import copies a trusted installed runtime because Proton may update its default
prefix and SteamPipe files. The CLI never patches Proton or Steam installations.
`supervisor.cpp` shares bounded execution, ancestry tracking, signal handling and
cleanup between Wine/Proton runtime tests and the game probe. Game probes opt
into bounded Wine module-load trace inspection and their profile's working
directory. Process-log write failures stop the owned launch; reparented children
retain ownership only through observed PID/start-time identity, and cleanup
reports remaining members. It is serialized within a CLI
process; future parallel GUI operations must use worker processes rather than
competing process-wide signal handlers.

The profile in `profiles/fortnite/12.61.toml` is embedded as a Qt resource, so an
installed binary does not depend on its original working directory. Native GUI,
backend adapter, host manager and server browser will consume the same core when
their prerequisites are verified. Windows implementation is explicitly deferred.

Build versions are strings, not floating-point values. Module architecture and
matching hashes do not prove functional compatibility. Every uncertain capability
is reported as unverified. No real Epic credentials are accepted or stored.

Direct diagnostic startup runs the ordinary shipping executable inside a new
unprivileged network namespace. This tests startup without exposing production
services. It does not provide a private backend or constitute a functioning
offline gameplay path. Ordinary `launch` stays blocked until such a path is
established. No security-bypass features from upstream are included.

Process logs contain names, PID, parent PID and process start ticks. They omit
unrelated command arguments and environment values. Shutdown uses pidfds and
start-time validation, never global wineserver/taskkill commands. Monitoring is
sampled and cannot prove that a short-lived process never existed.
