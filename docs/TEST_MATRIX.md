# Compatibility matrix

Results as of 2026-09-29. Synthetic tests do not count as game compatibility.

| Environment | Native CLI | 64-bit Vulkan enumeration | 12.61 detection | Game/Reboot/join/host | Graphics/audio/input/fullscreen/EAC/shutdown |
| --- | --- | --- | --- | --- | --- |
| CachyOS + NVIDIA RTX 4070 SUPER + Hyprland | Passed | Passed | Synthetic tests only | Blocked: no 12.61 files | Game behavior untested |
| CachyOS + AMD iGPU + Hyprland | Same machine | Passed | Synthetic tests only | Blocked | Game behavior untested |
| CachyOS + NVIDIA + KDE Wayland | Untested | Untested | Untested | Untested | Untested |
| Arch + NVIDIA | Untested | Untested | Untested | Untested | Untested |
| Arch + AMD | Untested | Untested | Untested | Untested | Untested |

Prefix initialization and offline Wine cmd.exe execution passed on the CachyOS
host, with zero owned processes after completion. Full supervisor tests using a
native synthetic runtime passed for failure capture and EAC-named test-process
termination; they do not execute EAC or Fortnite. 32-bit loader presence
was checked, but 32-bit device execution remains untested. No claim is made for
DXVK/VKD3D, rendering, performance, monitor changes, DPI, focus or mouse capture.
