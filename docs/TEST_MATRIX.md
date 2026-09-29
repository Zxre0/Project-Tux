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

| CachyOS command-only test | Execution | Cleanup | Scope |
| --- | --- | --- | --- |
| Wine 11.18 | Passed, including after reset and restore | No owned processes remain | Built-in cmd.exe, offline namespace |
| Proton Experimental 11.0-20260924 | Passed twice with fresh Windows-written receipts | No owned processes remain | Imported copy, dedicated compatdata, offline namespace |
| Proton Hotfix 20260828-ptr | Passed with Windows-written receipt | No owned processes remain | Separate imported copy and compatdata, offline namespace |

These tests do not rank runtime performance or prove any Fortnite startup stage.

## Supplied 12.50 startup diagnostics (not 12.61 compatibility)

| Runtime experiment, CachyOS/Hyprland host | Startup result | Cleanup/anti-cheat observation | Scope |
| --- | --- | --- | --- |
| System Wine 11.18, owned 12.50 prefix | Engine shutdown, exit 1, cause unknown | No owned processes left; no anti-cheat observed | Repeated isolated traces, no verified rendering/menu/gameplay |
| Experimental 20260924 bundled Wine 11.0, separate plain-Wine prefix | Missing template VKD3D DLLs fixed; engine shutdown, exit 1 | No owned processes left; no anti-cheat observed | Direct bundled Wine only, not full Proton/Steam Runtime integration |
| Proton Experimental 20260924 script, separate Proton prefix | Real Shipping load, launcher exit 1; game exit 1 also traced | No owned processes left; no anti-cheat observed | Host-script probe, not Steam Linux Runtime container; xalia DXVK output is not game rendering |
| Proton Hotfix 20260828 script, separate Proton prefix | Real Shipping load, launcher exit 1 | No owned processes left; no anti-cheat observed | Host-script probe, not verified rendering/menu/gameplay |

See [runtime evidence](RUNTIME_COMPARISON.md). Observed absence is sampled, not
absolute attestation. No graphics/audio/input/fullscreen functionality is claimed.
