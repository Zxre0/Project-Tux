# Compatibility matrix

Results as of 2026-09-30. Only12.50 is an active target.
Synthetic tests do not count as game compatibility.

| Environment | Native CLI | 64-bit Vulkan enumeration | 12.50 detection | Game/Reboot/join/host | Graphics/audio/input/fullscreen/EAC/shutdown |
| --- | --- | --- | --- | --- | --- |
| CachyOS + NVIDIA RTX 4070 SUPER + Hyprland | Passed | Passed | Supplied build verified | Lawin lobby/outfits verified; game server/join/host unverified | DXVK/XWayland and synthetic UI verified; audio untested; sampled EAC/cleanup evidence below |
| CachyOS + AMD iGPU + Hyprland | Same machine | Passed | Same installation | GPU-specific game behavior untested | Game behavior untested |
| CachyOS + NVIDIA + KDE Wayland | Untested | Untested | Untested | Untested | Untested |
| Arch + NVIDIA | Untested | Untested | Untested | Untested | Untested |
| Arch + AMD | Untested | Untested | Untested | Untested | Untested |

Prefix initialization and offline Wine cmd.exe execution passed on the CachyOS
host, with zero owned processes after completion. Full supervisor tests using a
native synthetic runtime passed for failure capture and EAC-named test-process
termination; they do not execute EAC or Fortnite. 32-bit loader presence
was checked, but 32-bit device execution remains untested. No claim is made for
DXVK/VKD3D, rendering, performance, monitor changes, DPI, focus or mouse capture
from those command-only tests. Later 12.50 rendering evidence is separated below.

| CachyOS command-only test | Execution | Cleanup | Scope |
| --- | --- | --- | --- |
| Wine 11.18 | Passed, including after reset and restore | No owned processes remain | Built-in cmd.exe, offline namespace |
| Proton Experimental 11.0-20260924 | Passed twice with fresh Windows-written receipts | No owned processes remain | Imported copy, dedicated compatdata, offline namespace |
| Proton Hotfix 20260828-ptr | Passed with Windows-written receipt | No owned processes remain | Separate imported copy and compatdata, offline namespace |

These tests do not rank runtime performance or prove any Fortnite startup stage.

## Supplied 12.50 startup diagnostics (historical baselines)

| Runtime experiment, CachyOS/Hyprland host | Startup result | Cleanup/anti-cheat observation | Scope |
| --- | --- | --- | --- |
| System Wine 11.18, owned 12.50 prefix | Engine shutdown, exit 1, cause unknown | No owned processes left; no anti-cheat observed | Repeated isolated traces, no verified rendering/menu/gameplay |
| Experimental 20260924 bundled Wine 11.0, separate plain-Wine prefix | Missing template VKD3D DLLs fixed; engine shutdown, exit 1 | No owned processes left; no anti-cheat observed | Direct bundled Wine only, not full Proton/Steam Runtime integration |
| Proton Experimental 20260924 script, separate Proton prefix | Real Shipping load, launcher exit 1; game exit 1 also traced | No owned processes left; no anti-cheat observed | Host-script probe, not Steam Linux Runtime container; xalia DXVK output is not game rendering |
| Proton Hotfix 20260828 script, separate Proton prefix | Real Shipping load, launcher exit 1 | No owned processes left; no anti-cheat observed | Host-script probe, not verified rendering/menu/gameplay |

See [runtime evidence](RUNTIME_COMPARISON.md). Observed absence is sampled, not
absolute attestation. The baseline probes above do not establish graphics/audio/
input/fullscreen functionality.

## Opt-in 12.50 launcher-context experiments

These have different arguments from the baselines, after explicit user authorization.
They remain isolated and cannot connect to production or a host private backend.

| CachyOS/NVIDIA/Hyprland | Startup/rendering | Remaining limitations |
| --- | --- | --- |
| Wine 11.18, session 18-28-07_e5588ee2 | Engine/pak/D3D11 initialization and logged sign-in activation | WineD3D buffer errors; no screenshot, login or gameplay verification |
| Proton Experimental host script, session 18-31-49_582d3fb7 | Game DXVK logs, NVIDIA selected, 1920x1080 XWayland sign-in window visually verified | No login/lobby/join/hosting; audio/input/monitor behavior untested; no performance benchmark |

Both ended at the supervisor timeout, with no owned processes remaining and no
EAC/BattlEye process/module observed. Game-internal UAC checks remain present.
See [12.50 evidence](FORTNITE_12_50.md). Other desktops remain untested;
a sign-in screenshot is not private gameplay compatibility.

## Isolated 12.50 backend discovery

| Selector experiment | Observed result | Interpretation |
| --- | --- | --- |
| MCPCONFIG=localhost | Normal production backend selection remains; requests isolated | Not a working redirect |
| epicapp=Localhost | Selector recognized; missing service configuration, host unconfigured, backend rejected | No working private backend; context-only sign-in baseline is a separate command |

The second experiment was repeated once to validate automatic structured game-log
reporting. Both runs cleaned up and had no observed EAC/BattlEye process/module.
No backend service, credentials, private login or server connection was tested.

## Isolated Lawin lobby verification (2026-09-30)

| Tested environment | Verified | Not established |
| --- | --- | --- |
| CachyOS, NVIDIA4070SUPER, Hyprland/XWayland, supplied12.50, Proton Experimental20260924 host script (Wine11.0), isolated Lawin V2 | Local password login, XMPP/party, rendered Athena lobby, Win32 synthetic Battle Royale and Locker/Play navigation, eight starter outfit thumbnails, male/female character rendering, UI outfit equip/save and restart persistence, no AppES marker through300s module window, scoped process cleanup | Physical input, audio, game-server join/hosting, long-running stability, KDE/AMD/other distributions |

The bootstrap-context options explicitly spoof the launch origin; see EAC.md.
No suspended launcher was needed in the two final tests. EAC/BattlEye was not
observed during sampling, not proven universally absent. The diagnostic helper
ends its owned game job at the observation bound; this is not a graceful user exit
test. Session a58bd11b provides the repeated menu-transition screenshots and logs.
