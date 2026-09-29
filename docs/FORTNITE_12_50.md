# Fortnite 12.50 diagnostic target

This is a separately supplied diagnostic build, not a substitute for 12.61.
The project's primary compatibility target remains 12.61. No private gameplay,
Reboot module compatibility or non-EAC authorization path is established for 12.50.

Supplied root: `/home/atlas/Documents/Fortnite/++Fortnite+Release-12`.
Ordinary Shipping and CrashReportClient both contain `++Fortnite+Release-12.50`;
Shipping also contains `++Fortnite+Release-12.50-CL-13137020`. The engine version
has not been inferred from Season 12. Structural validation passes for x64 PE,
Engine/FortniteGame folders and readable nonempty pak content. All 398 files
listed in the supplied local Epic manifest also match their sizes and SHA-1
hashes (90,166,901,135 bytes checked). The manifest is not authenticated; this is
content-consistency evidence, not provenance or gameplay compatibility.

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

Targeted trace command:
`fortnite-linux launch 12.50 --offline-probe --trace-startup --timeout 35`.
This adds Wine `msgbox`, `debugstr`, `process`, `exec` and `reg` channels; executable and
game arguments are unchanged. Wine's [message-box implementation](https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/user32/msgbox.c)
contains a dedicated text trace, allowing a hidden/transient dialog to be recorded.
It does not install a hook or bypass a launch requirement.

That probe was temporarily **environment-blocked**. After the execution environment
changed to a restricted sandbox, Vulkan instance creation succeeded but physical
device enumeration returned -3 and no devices; `/dev/dri`, `/dev/nvidia0` and
`/dev/nvidiactl` are absent from that environment. Local socket creation also
returns EPERM. Session `2026-09-29_13-37-40_7e950a34` stopped before Wine/game start.
Permissions were subsequently restored; see the completed probes below. The
Vulkan gate and network namespace were retained throughout.

## Restored-access startup probes

All three sessions used Wine 11.18 and the same dedicated 12.50 prefix. Every run
exited 1 without a Unix crash or timeout, left no owned processes, and recorded no
EAC/BattlEye process or matching module-load evidence. No gameplay was verified.

| Session (UTC, 2026-09-29) | Difference from baseline | Finding |
| --- | --- | --- |
| `17-38-00_3e18f314` | Wine msgbox/debugstr trace | No explanatory dialog/debug message; same engine teardown |
| `17-39-23_45189492` | Added Wine process/exec trace | No game shell-execution handoff captured; same exit |
| `17-41-08_8e7cac88` | Added `--engine-stdout` | No additional engine startup diagnostics captured; same exit |

Wine's [process implementation](https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/kernelbase/process.c)
uses the `process` channel; [shell execution](https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/shell32/shlexec.c)
uses `exec`. These traces observe existing behavior; they do not create launcher
stand-ins, modify return values or bypass any checks.

`--engine-stdout` separately requests `-stdout -FullStdOutLogOutput -forcelogflush`.
All three strings occur in the supplied Shipping binary. Epic's
[argument reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-command-line-arguments-reference)
documents stdout output and line flushing generically; it is not proof this
Shipping build enables logging at the failed startup stage. Output remains in
the session's `wine.log`; the launcher reports only that logging was requested.
Neither this experiment nor the observed LauncherCheck teardown proves an
authentication requirement. The exact exit cause is still unknown.

A subsequent direct bundled-Wine comparison fixed an experimental-prefix missing
VKD3D dependency, then reached the same exit-1 engine shutdown. It is not a full
Proton integration test; see [runtime comparison](RUNTIME_COMPARISON.md).

## Launcher-discovery observation

Session `2026-09-29_17-54-32_619a143a` added Wine's `reg` channel to the system-Wine
baseline. It captured a failed lookup immediately before engine shutdown:

```text
NtOpenKeyEx (0xac,L"com.epicgames.launcher",20019,...)
NtOpenKeyEx <- (nil)
```

The preceding same-thread trace identifies handle `0xac` as
`\Registry\Machine\Software\Classes`. This is a failed discovery lookup for the
Epic launcher registration, not just a module-teardown name. Exit remains 1,
without timeout/Unix crash, observed anti-cheat or owned processes remaining.
Wine's [registry implementation](https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/registry.c)
traces key-open arguments and the returned handle through `reg`.

The temporal association is **not causal proof**, an authorization verdict, or
evidence that EAC is fundamentally required. No registration was fabricated, no
Epic-launched identity flag added, and no protected check patched. The probe
does not establish a supported private/offline path. Further integration needs
evidence of an authorized development launch contract, not a fake launcher entry.
The trace option remains opt-in; raw registry logs stay outside Git.

## Local manifest verification

The supplied `.egstore/EA2F274841C3E0693C3E06BFD2CBA2DE.manifest` reports
`++Fortnite+Release-12.50-CL-13137020-Windows`, agreeing with embedded PE metadata.
It is an unencrypted zlib-compressed container v18 with metadata feature level 13.
Its launcher field names `FortniteLauncher.exe`; this is reported only and is not
used by the private diagnostic launch path.

Read-only verification checked all 398 entries, including tagged components,
in 72.658 seconds. Results: 398 matched, zero missing, zero mismatched, internal
manifest payload hash valid. The full machine-local report is
`~/.local/state/fortnite-on-linux/logs/12.50-manifest-hashes.json`.
No game files, manifest contents or anti-cheat files were changed. This reduces
ordinary corruption as a hypothesis for the observed exit but does not identify
the cause or validate a legitimate offline execution path.
