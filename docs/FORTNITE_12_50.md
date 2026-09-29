# Fortnite 12.50 diagnostic target

This is a separately supplied diagnostic build, not a substitute for 12.61.
The project's primary compatibility target remains 12.61. No private gameplay,
Reboot module compatibility or non-EAC authorization path is established for 12.50.

Supplied root: `/home/atlas/Documents/Fortnite/++Fortnite+Release-12`.
Ordinary Shipping and CrashReportClient both contain `++Fortnite+Release-12.50`;
Shipping also contains `++Fortnite+Release-12.50-CL-13137020`. The engine version
has not been inferred from Season 12. Structural validation passes for x64 PE,
Engine/FortniteGame folders and readable nonempty pak content; full hashes against
an authenticated manifest have not been checked.

Observed ordinary Shipping SHA-256:
`34a10af4ddff20b75d889e269a66644d570c3000f13548a5799d3a7ef690190c`.
This records the supplied file, not a published authenticity reference.

`profiles/fortnite/12.50.toml` selects only ordinary Shipping with `-log -d3d11`.
Use `launch 12.50 --offline-probe`; the network namespace has no production route
and no host backend connectivity. Dedicated prefix: XDG data `prefixes/12.50`.
Do not invoke Shipping_EAC, Shipping_BE, FortniteLauncher or either anti-cheat
setup/service. Both bootstrap configs point to the ordinary executable, but that
alone does not prove an accepted private/offline execution path.

The first run reached engine shutdown, including LauncherCheck/LauncherPlatform
module teardown, but a launcher cleanup-scope defect prevented a reliable final
result. That defect and possible unrelated-process impact are recorded in
[PROGRESS.md](PROGRESS.md). No game-startup success is claimed from that run.

## Corrected Wine baseline

Session `2026-09-29_13-35-35_66361806` ran the same ordinary executable through
Wine 11.18 with corrected process selection. The game exited with code **1**;
the recorded status is `exited; gameplay-unverified`. The process log contains
165 samples, no PID 0/1, and at most nine launch-related processes per sample.
No EAC/BattlEye process or matching Wine module-load trace was observed. This is
sampled evidence, not an unconditional assertion that no anti-cheat ever loaded.
Prefix status after the run reported zero active processes.

The log contains engine exit and LauncherCheck/LauncherPlatform teardown, along
with Wine service/RPC and Mesa EGL warnings. None of these, alone, proves why the
game exited. In particular, module teardown is not proof of an authentication
failure, and graphics warnings must not be assumed causal without more evidence.

Next targeted probe:
`fortnite-linux launch 12.50 --offline-probe --trace-startup --timeout 35`.
This adds only Wine `msgbox` and `debugstr` channels; executable and game arguments
are unchanged. Wine's [message-box implementation](https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/user32/msgbox.c)
contains a dedicated text trace, allowing a hidden/transient dialog to be recorded.
It does not install a hook or bypass a launch requirement.

That probe is currently **environment-blocked**. After the execution environment
changed to a restricted sandbox, Vulkan instance creation succeeded but physical
device enumeration returned -3 and no devices; `/dev/dri`, `/dev/nvidia0` and
`/dev/nvidiactl` are absent from that environment. Local socket creation also
returns EPERM. Session `2026-09-29_13-37-40_7e950a34` stopped before Wine/game start.
The exact game-exit cause therefore remains unresolved pending suitable device
access. Do not remove the Vulkan gate or bypass the sandbox to force a retest.
