# Fortnite 12.50 diagnostic target

This is a separately supplied diagnostic build, not a substitute for 12.61.
The project's primary compatibility target remains 12.61. No private gameplay,
Reboot module compatibility or non-EAC authorization path is established for 12.50.

Supplied root: `/home/atlas/Documents/Fortnite/++Fortnite+Release-12`.
Ordinary Shipping and CrashReportClient both contain `++Fortnite+Release-12.50`;
Shipping also contains `++Fortnite+Release-12.50-CL-13137020`. Static inspection
does not identify the engine version; the later runtime log below reports 4.25.0.
Neither result is inferred from Season 12. Structural validation passes for x64 PE,
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

Epic's current [ILauncherCheckModule API reference](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/LauncherCheck/ILauncherCheckModule)
identifies responsibilities for checking whether an app came from the launcher
and requesting launcher execution. This corroborates the module's architectural
role, not the implementation or supported command line of Fortnite 12.50. The
current API page does not establish a version-specific private/offline contract;
no launch-origin flags are inferred or added from it.

## Offline-mode prerequisite check

On 2026-09-29, rechecked the supplied loose configuration/documentation filenames
and the project-owned baseline Wine and Experimental Proton prefixes. No separate
development-launch instructions were identified among those supplied files. A
filename search found no `*Fortnite*.log`, `*CrashContext*.xml` or `*UE4Minidump*`
in the installation or those two prefixes; this does not exclude differently
named diagnostics or information inside packaged assets. No assets were unpacked
or modified, and no account data was accessed.

Installing the real Epic Games Launcher just to use its offline mode is not an
evidence-backed route to this project's target. Epic's
[2019 offline-mode announcement](https://store.epicgames.com/news/launcher-offline-mode?lang=no)
explicitly excludes Fortnite from games that work offline. Its current
[Fortnite offline-mode support answer](https://www.epicgames.com/help/c-34254770/c-33726977/a14084939)
also says offline play is unavailable. The announcement predates 12.50, whereas
the current support answer is not a version-specific specification. Neither is
proof of the observed exit's cause, an EAC requirement, or the impossibility of
an independently authorized private development build.

The missing prerequisite is a verifiable 12.50 development execution contract:
which authorized executable/build accepts private operation, its supported
arguments, its backend/server interface, and how it avoids starting anti-cheat
without replacing authentication or bypassing checks. A retail installation,
generic Unreal API documentation, or a Reboot launcher chain that starts paused
EAC/authentication replacement components does not supply that contract. Do not
invent registry entries or launcher-origin assertions to make this prerequisite
appear satisfied. Until new evidence provides an in-scope experiment, retain the
captured exit-1 result rather than retrying unchanged runtimes or changing drivers.

## Upstream OpenSSL-only comparison

User-requested renewed source inspection identified a separate upstream environment
setting that can be tested without its authentication/anti-cheat chain. Session
`2026-09-29_18-23-55_26904814` used system Wine 11.18, the same baseline prefix,
network isolation and unchanged `-log -d3d11` arguments, adding only the upstream
`OPENSSL_ia32cap=~0x20000000` value:

```sh
env OPENSSL_ia32cap='~0x20000000' fortnite-linux launch 12.50 --offline-probe --trace-startup --timeout 35
```

Result: exit 1, no Unix crash/timeout, same null launcher-key lookup on Wine
PID 0020/thread 0024 followed by LauncherPlatform/LauncherCheck teardown. No
anti-cheat process/module evidence was observed, cleanup completed, and prefix
status afterwards reported zero active processes. The CLI itself returned 2 to
report unsuccessful gameplay-unverified execution; the game/runtime exit was 1.
Shipping and Aftermath SHA-256 values matched before/after; neither was modified.

This exact upstream environment setting alone did not advance startup. That does
not prove OpenSSL consumed it or rule out every possible CPU/library problem.
No capability mask is enabled by default. The launcher now explicitly includes
an inherited OPENSSL_ia32cap value in its command/result receipt for reproducible
diagnostics; it previously inherited it without recording that detail.
See [upstream stage analysis](REBOOT_LAUNCHER_ANALYSIS.md) for the distinction
between this experiment and the excluded upstream launch/authentication sequence.

## Local manifest verification

See the later launcher-context experiment below: baseline exit findings above
remain valid for their original arguments, not for every possible local command.

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

## Explicitly authorized launcher-context experiment

On 2026-09-29 the user broadened authorization for local startup experiments.
Added opt-in `--launcher-context-probe`, defined only by the 12.50 compatibility
profile and requiring `--offline-probe`. It adds the single `-epicportal` context
argument observed in upstream, without authentication fields/tokens, modules,
launcher/EAC processes, registry fabrication or binary changes. This intentionally
changes the launcher-context assertion, unlike the earlier baseline. It is not
proof that an official launcher initiated this process or that private gameplay
is authorized/supported. The baseline and 12.61 profile are unchanged.

System Wine 11.18 session `2026-09-29_18-28-07_e5588ee2` with the OpenSSL mask
unset progressed beyond the prior exit-1 stage. A new FortniteGame.log records
engine/D3D11 initialization, selection of NVIDIA RTX 4070 SUPER, game-feature
loading, transition to manual login and SignIn_Credentials focus. The engine
loaded the unchanged Aftermath DLL. These are engine-log observations, not visual
confirmation of a usable menu. WineD3D logged repeated GL_INVALID_ENUM invalid
buffer format errors; graphical correctness remains unverified.
The same engine log reports `4.25.0-13137020+++Fortnite+Release-12.50` and
compatible engine `4.25.0-13136868+++Fortnite+Release-12.50`. This is observed runtime
metadata, not a new hardcoded static-detection result.

The engine reported UpdateFailure_PatchCheck and inability to autologin while
production endpoints were unreachable in the isolated namespace. No credentials
were provided. The game also logged a UAC direct-launch warning; no claim of
disabled game-internal checking is made. No EAC/BattlEye process/module was
observed by the existing monitor.

The game remained active until the 45-second supervisor timeout. Result status
is probe-timeout, exit 15; the current raw `crashed: true` reflects QProcess's
signal-exit classification after controlled SIGTERM, not evidence of a spontaneous
game crash. Process capture: 786 samples, at most nine owned processes, no PID 0/1,
and cleanup complete. The game log was copied into the session as fortnite.log
before another run could overwrite it. Next justified comparison is Proton/DXVK
with the same context argument to investigate the observed WineD3D errors.

### Proton/DXVK sign-in screen verified

Session `2026-09-29_18-31-49_582d3fb7` used imported Experimental via its host
script, the same opt-in context argument, no OpenSSL mask and a 60-second limit.
Only baseline Wine SEH/module traces were enabled, not verbose registry/process
tracing. The exact command is preserved in the session. The game now generated
its own DXVK logs (not just xalia's): DXVK v3.1.1-27-g25ca63f17f34bdc; game engine
selected NVIDIA RTX 4070 SUPER. No matches for the preceding WineD3D GL_INVALID_ENUM
or err:d3d messages occurred in this Wine log. This does not establish all graphics
features or a performance advantage.

A Hyprland XWayland window named Fortnite, class steam_app_0, PID 63403 was mapped
at 1920x1080. After checking that it was the active window and occupied exactly
that rectangle, a local screenshot was captured and visually inspected: a rendered
12.50 (13137020) sign-in screen with empty email/password fields. No credentials,
input automation or production connection were used. Screenshot fortnite-window.png,
copied fortnite.log and game-specific DXVK logs remain in the session, not Git.

The game log again reports offline connection/patch-check failure, manual sign-in
activation and the UAC direct-launch warning. Wine traces include handled
illegal-instruction exceptions; the continuing rendered screen and handler traces
do not establish an unhandled CPU crash. Audio fallback/GfeSDK errors also remain;
audio and interactive input have not been validated. The supervisor stopped the
run at its limit: 1043 samples, max 13 owned processes, no PID 0/1, no EAC/BattlEye
process/module observed, and cleanup complete. This verifies rendering to sign-in,
not successful login, lobby, private-server join, hosting or gameplay.

Repeatability check `2026-09-29_18-38-39_a26403a9` used the same Proton/context
configuration with a 35-second bound. The engine again logged manual sign-in
activation. Automatic collection preserved 287004 bytes as session fortnite.log;
the result recorded `stop_requested: true` and explained that raw QProcess
CrashExit during cleanup is not a spontaneous-crash diagnosis. Capture was
614 samples/max 13 owned processes/no PID 0/1; no anti-cheat was observed and no
owned processes remained. Shipping SHA-256 is unchanged. This repeat validates
log preservation/controlled-stop reporting, not additional gameplay behavior.

## Built-in backend selection discovery

The supplied binary contains MCPCONFIG=, McpConfigOverride and backend-selection
diagnostics. These strings suggested testing the built-in option, not assuming
that it is a working private-backend API. The first --local-backend-probe revision
added only `-MCPCONFIG=localhost` to the context probe. It is restricted to this
profile and requires both context and network-isolation opt-ins. No backend is
started. The current experiment instead uses `-epicapp=Localhost`: the binary
explicitly contains an epicapp-override diagnostic and a Localhost entry next to
its named backends. These are distinct experiments, not equivalent spellings.

Session `2026-09-29_18-46-46_f124f659`, Experimental host script, 35 seconds:
the engine's filtered command line includes the new argument, but its backend
selection still names the normal production Fortnite host, environment Prod and
empty override. It returns to the same logged sign-in stage; production requests
remain unreachable. Therefore the option/value alone did **not** redirect this
build. No authentication/TLS changes or module loading were used. Do not promote
this experiment to a supported --server command or loosen network isolation.

Automatic capture saved 287026 bytes of game log. The supervisor requested the
timeout stop, recorded 598 process samples (max 13, no PID 0/1), observed no
EAC/BattlEye process/module and left no owned processes. This is a negative routing
finding, not a new startup failure, login success or host compatibility result.

Follow-up session `2026-09-29_18-53-34_b7a20b71` tests the distinct
`-epicapp=Localhost` selector with the same Experimental runtime and 35-second
bound. It is recognized: the game selects backend Localhost, environment GameDev,
override Localhost, but reports host `<unconfigured>`, a missing Fortnite MCPConfig
for service fortnite, then `Invalid backend requested! Exiting.` It does not reach
the prior manual-sign-in log marker. Thus this build recognizes the selector but
does not supply the corresponding usable service configuration. No backend was
started and no authentication or certificate behavior was modified. This does not
prove that a supported configuration mechanism cannot exist; it rules out this
argument alone as a functional private path.

Automatic capture preserved 141314 bytes. The runtime remained until the bounded
supervisor stop (not proof that the game exited itself despite its log message).
No EAC/BattlEye process/module was observed and owned-process cleanup completed.
