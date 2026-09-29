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
