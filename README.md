# FortniteOnLinux

Native Linux CLI/core for investigating Fortnite **12.61** with legitimately
obtained files and authorized local/private development infrastructure.
Canonical repository: <https://github.com/Zxre0/FortniteOnLinux>.

**Current status: experimental foundation; Fortnite startup, Project Reboot
integration and gameplay have not been verified.** No complete 12.61 installation
was found on the development machine. A legitimate non-EAC private execution
path has not been established. This project does not bypass authentication,
DRM, SSL pinning or anti-cheat, and distributes no Fortnite assets.

## Build

Requires C++20, CMake >=3.22, Ninja, Qt >=6.4 Core/Network, toml++ >=3.3,
and Vulkan headers/loader development files. No Wine is needed to run the CLI
or asset-free tests. On Arch/CachyOS developer packages are `base-devel cmake
ninja qt6-base tomlplusplus vulkan-headers vulkan-icd-loader`.

```sh
./build.sh
./build/fortnite-linux --help
./build/fortnite-linux diagnose
```

If using a separately provisioned Khronos header checkout, configure once with
`cmake -S . -B build -G Ninja -DVulkan_INCLUDE_DIR=/absolute/path/to/Vulkan-Headers/include`.
`./build.sh` preserves that CMake cache setting. See [Linux notes](docs/LINUX.md).

## Current commands

```sh
fortnite-linux discover /mnt/games
fortnite-linux add /path/to/Fortnite
fortnite-linux builds
fortnite-linux verify 12.61
fortnite-linux remove 12.61              # registration only; preserves files
fortnite-linux runtime list
fortnite-linux prefix create 12.61
fortnite-linux prefix status 12.61
fortnite-linux diagnose --output system.json
fortnite-linux launch 12.61 --offline-probe --print-command
fortnite-linux launch 12.61 --offline-probe --timeout 60
fortnite-linux module /path/to/module.dll --manifest /path/to/module.toml
fortnite-linux network 127.0.0.1 7777
fortnite-linux processes
fortnite-linux logs
```

Registration supports multiple IDs (`add PATH --id custom-name`). Detection reads
ASCII/UTF-16 release metadata and checks PE architecture, section bounds, content
folders and readable nonempty paks. It does **not** prove asset authenticity or
full integrity. Conflicting/unknown metadata is rejected. No unofficial downloads.

Normal `launch` and `host` currently report a blocker. `--offline-probe` is only a
network-isolated direct startup experiment using installed Wine, not working
offline gameplay. It requires unprivileged Linux user/network namespaces and an
initialized dedicated prefix. It never starts the EAC bootstrap intentionally.
Process-name monitoring is sampled; absence from snapshots is not proof that EAC
never loaded. See [EAC findings](docs/EAC.md).

The core detects Steam-local Proton installations but does not launch through
Proton yet. DXVK/VKD3D installation, runtime downloads, complete prefix lifecycle,
private backend/server integration, server browser, dev-loop and GUI remain
unimplemented. WineD3D is the unchanged Wine baseline. Gamescope is optional and
currently only detected. No production services or router configuration are used.

Configuration, data, cache and logs follow XDG conventions under
`fortnite-on-linux`. Every launch request records a unique session under
`~/.local/state/fortnite-on-linux/logs/sessions/`, including blocked preflight.
The CLI prints JSON and returns nonzero on validation or launch failure.

See [progress](docs/PROGRESS.md), [architecture](docs/ARCHITECTURE.md),
[12.61 research](docs/REBOOT_12_61.md), [compatibility matrix](docs/TEST_MATRIX.md),
and [troubleshooting](docs/TROUBLESHOOTING.md).
