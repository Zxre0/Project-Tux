# Linux implementation and measurements

Primary target is CachyOS/Arch on x86_64, Wayland, NVIDIA, with AMD secondary.
Qt Core/Network runs as native ELF with no display requirement. GUI work is
deferred until the launch path works. XWayland game behavior remains untested.

2026-09-29 machine: CachyOS, kernel 7.2.8-1-cachyos, Hyprland/Wayland, NVIDIA
RTX 4070 SUPER (PCI 10de:2783, nvidia driver), AMD integrated graphics
(1002:13c0, amdgpu). Vulkan instance creation and enumeration succeeded for both.
32-bit loader exists as ELF32; 32-bit Vulkan device execution is not tested.
Installed NVIDIA userspace packages report 615.71.09, including lib32-nvidia-utils.
No driver, bootloader, Secure Boot or system service settings were changed.

Wine 11.18 is installed. Steam has Proton Experimental
`experimental-11.0-20260924-x86_64` and Proton Hotfix `hotfix-20260828-ptr-x86_64`.
UMU, Gamescope and GameMode were not found in PATH. No evidence currently favors
GE-Proton or any other runtime for this build. Vulkan success is enumeration,
not a rendering, DXVK, audio or input test.

Initial build failed because Vulkan headers were missing. `sudo -n pacman`
required a password, so no package was installed. Used official Vulkan-Headers
v1.4.304, commit `d4a196d8c84e032d27f999adcea3075517c1c97f`, cloned into the XDG
project cache and explicitly supplied to CMake. Qt 6.11.2, GCC 16.2.1 and
toml++ 3.4.0 were already installed. The CLI dynamically resolves Vulkan loader
symbols, permitting diagnostic reporting when the runtime loader is absent.

Arch/CachyOS package names relevant to a missing runtime: `wine`,
`vulkan-icd-loader`, `lib32-vulkan-icd-loader`, NVIDIA `nvidia-utils` and
`lib32-nvidia-utils`, AMD `vulkan-radeon` and `lib32-vulkan-radeon`. Package
presence is not proof of a working device. Multilib needs distribution-supported
configuration; the launcher does not edit pacman configuration or install drivers.

Data: `$XDG_DATA_HOME/fortnite-on-linux` (default `~/.local/share/...`).
Config: `$XDG_CONFIG_HOME/fortnite-on-linux` (default `~/.config/...`).
Cache: `$XDG_CACHE_HOME/fortnite-on-linux` (default `~/.cache/...`).
Logs: `$XDG_STATE_HOME/fortnite-on-linux/logs` (default `~/.local/state/...`).
Relative XDG environment values are ignored. Prefix IDs cannot traverse paths;
symlinked prefix components and unowned directories are rejected.

`runtime test runtime-smoke` passed with Wine 11.18 in an unprivileged user/network
namespace, printing Microsoft Windows 10.0.19045 and leaving zero owned processes.
The wineserver wait must run inside the same user namespace: its socket identity
depends on effective UID, so a host-side wait can incorrectly report completion.
Log session: `runtime-fcfb4677-b274-4b7c-a8cc-af24990951e1`. Wine logged a winebth
service-start error in the earlier run; the built-in command still executed.
This is not a graphics or Fortnite test.

After the environment changed to a restricted sandbox, a display-free retest
(`env -u DISPLAY -u WAYLAND_DISPLAY fortnite-linux runtime test runtime-smoke`)
failed before Windows command execution. Session
`runtime-4ad5f6d6-464a-46ac-ad2c-03036090db01` records exit 1, no execution receipt,
`sock_init: ERROR in sock_check_pollhup()` and
`wineserver: bind: Operation not permitted`. No owned processes remained.
This rules out treating a headless render option as a solution to the current
environment problem: Wine server IPC must work even without a GPU or display.
It does not prove a Fortnite-specific launch restriction.

`diagnose.local_ipc` now tests an AF_UNIX stream socket bind/listen in a newly
created private temporary directory, then closes/removes only its own resources.
It sends no traffic and never touches existing Wine sockets or prefixes. It
reports the precise failing operation and errno; the current sandbox returns
EPERM at `bind`, independently matching the Wine error. Success is only a native
IPC prerequisite, not proof that Wine or the game can execute. The `ipc` CTest
target requires real bind/listen success and is not suppressed in CI/full builds.

Diagnostics now report installed Arch-family package versions and vendor-specific
package suggestions. Names were cross-checked against local pacman evidence and
the official [Vulkan loader](https://archlinux.org/packages/extra/x86_64/vulkan-icd-loader/),
[32-bit NVIDIA](https://archlinux.org/packages/multilib/x86_64/lib32-nvidia-utils/),
and [32-bit Radeon](https://archlinux.org/packages/multilib/x86_64/lib32-vulkan-radeon/)
package records. Suggestions do not install or upgrade drivers. 32-bit libraries
are labelled runtime-dependent; their necessity is not inferred from a 64-bit game.

Prefix status inspects dxgi, d3d11, d3d12, vcruntime140 and msvcp140 for readable
x64 DLL structure. All five were present in the Wine-created 12.61 prefix. This
does not prove DXVK/VKD3D or Microsoft VC++ is installed: Wine supplies its own DLLs.

## Recoverable prefix operations

`prefix backup ID` copies an inactive owned prefix using GNU coreutils
`cp --archive --reflink=auto`; symlinks are copied, not traversed. `archive` moves
it into project-owned backup storage. `reset` archives first, then initializes a
fresh Wine prefix. Failed initialization includes the recovery location in its
error. Restore requires `prefix restore ID TOKEN` with no existing active prefix;
it consumes that backup via rename, retaining its metadata as a recovery record.
No recursive deletion is performed. Backup directories are owner-only; partial
copies are explicitly incomplete. These are same-user recovery safeguards, not a
security boundary against a malicious process running as the same user.

Real `runtime-smoke` backup, reset, archive and restore passed on 2026-09-29.
Both the replacement and restored prefixes subsequently passed the offline Wine
command test with zero remaining owned processes. Game files were not involved.

## Proton command baseline

The installed Experimental and Hotfix scripts use `STEAM_COMPAT_DATA_PATH/pfx`,
append a slash to WINEPREFIX, update prefix files during `run`, and invoke a Steam
shim before the requested Windows program. `waitforexitandrun` waits before the
program, not after; the launcher waits for the bundled wineserver in the same
user/network namespace after `run` instead. Script inspection was performed on
the exact installed versions, not assumed from a generic Wine invocation.

CLI import stores copies under `runtimes/ID` with source, version and combined
script/version SHA-256 metadata. This detects those files changing, not runtime
authenticity or every corrupted library. Use only trusted runtime installations.
Updates require new IDs; tests reject switching an existing Proton prefix to a
different runtime. `runtime list` shows local discovery and imported copies.
Proton's own default-prefix/fixup mutations are confined to its imported copy.

Test data lives in `proton-prefixes/ID`, separate from ordinary Wine prefixes.
The smoke test supplies an empty project-owned Steam-client directory, neutral
app ID 0 for log naming, and no Steam account/credentials. It clears inherited
Steam/Proton/Wine/media settings (some upstream media paths can trigger migrations),
rejects custom executable user settings, and launches only built-in cmd.exe in
a network namespace. Successful receipt comparison, exit status and process drain
are independent checks. Process ownership normalizes the trailing slash without
using an unsafe prefix-string match. Logs contain the reproducible command and
sampled process names/ancestry, not unrelated process arguments.

Experimental `experimental-11.0-20260924-x86_64` passed twice; Hotfix
`hotfix-20260828-ptr-x86_64` passed separately. The logs still contain winebth
driver/service errors and `Failed to connect to Steam`; these did not prevent
the isolated command receipt or clean shutdown. They are not suppressed or
attributed as Fortnite crash causes. No D3D workload or Fortnite was executed.
