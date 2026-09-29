# EAC/private-development boundary

No legitimate non-EAC private gameplay path has been verified. A supplied 12.50
build is undergoing isolated diagnostics; 12.61 files remain unavailable.

Expected upstream executable names are `FortniteClient-Win64-Shipping.exe`,
`FortniteClient-Win64-Shipping_EAC.exe`, `FortniteLauncher.exe`, and
`CrashReportClient.exe`. EAC detection also recognizes EasyAntiCheat-prefixed
executables/services, `start_protected_game.exe` and `eac_launcher.exe`.
The supplied 12.50 build also includes Shipping_BE.exe, BattlEye/BEService_x64.exe,
BEClient_x64.dll and EAC setup/x64/x86 DLLs. EAC Settings.json and BattlEye
BELauncher.ini both target ordinary Shipping. Neither bootstrap is invoked.

Reboot Launcher's game-start button creates paused launcher and EAC processes,
then the shipping process and an authentication module. That is not an acceptable
launch path for this project. No EAC process, suspended stand-in, authentication
module, integrity patch or SSL bypass is implemented here.

The diagnostic probe chooses only the ordinary x64 shipping executable, with
`-log -d3d11`, in a separate Linux network namespace. Direct invocation alone
does not prove that the game will accept an offline development session. The game
may refuse startup or authentication; such refusal must remain a documented
blocker, not be patched away. The namespace has no route to production services
or host private servers; this phase tests startup only.

Session monitor samples readable /proc process names/arguments for EAC names,
records process ancestry and start ticks, and stops its owned launch if EAC is
observed. It uses a dedicated WINEPREFIX and session marker, and pidfds when
signalling. It does not kill other Wine applications. Polling can miss short-lived
processes, unreadable processes or renamed modules; therefore status says
`not observed`, not an unconditional `Not Loaded`. The probe also inspects Wine
loaddll traces for EAC/BattlEye module-load evidence and stops on a match; this is
not complete module attestation. Preflight without execution reports `not tested`.

2026-09-29 baseline: no EAC-named processes observed; this is a system snapshot,
not a Fortnite 12.61 result. The first launch request failed before execution due
to missing build registration and has a persisted session result.

Restored-access 12.50 traces on 2026-09-29 at 17:38, 17:39 and 17:41 UTC all
exited 1 with no sampled EAC/BattlEye process or matching module-load evidence
and no owned processes remaining. Logging-only trace/engine-output options did
not identify the exit cause. These results do not establish an authorized
private gameplay path or prove an anti-cheat requirement. See
[12.50 evidence](FORTNITE_12_50.md) for exact sessions and argument differences.

Session `2026-09-29_17-54-32_619a143a` observed an unsuccessful
`HKLM\Software\Classes\com.epicgames.launcher` lookup before exit 1. This narrows
startup investigation to include launcher discovery; it does not prove an EAC or
authentication failure. No fake launcher registration, Epic-launched flag, paused
process stand-in or security patch was used to get past the lookup.

Proton-script host probes on Experimental and Hotfix (sessions
`2026-09-29_18-03-19_8c56af6f` and `2026-09-29_18-05-13_60a3ba6d`) started real
Shipping and reproduced the failed lookup/exit 1. The same anti-cheat process and
module-trace monitor was active: no matches observed, no owned processes left.
Explicit Wine tracing stayed in the supervised log instead of Proton's separate
log redirection. An external Steam runtime service handoff is rejected in this
mode to preserve the namespace boundary. These remain offline startup probes,
not verified private gameplay or an unconditional EAC-absence attestation.

After the user explicitly broadened local experiment authorization, session
`2026-09-29_18-28-07_e5588ee2` tested only an opt-in launcher-context argument in
the isolated 12.50 Shipping process. Unlike the earlier baseline, this changes
the declared launcher context; no upstream authentication module, token, paused
helper process or registry stand-in was used. The normal launch profile remains
unverified and this experiment is rejected without explicit network isolation.

The engine reached a logged sign-in stage. No EAC/BattlEye process or matching
module trace was observed. The game log does contain `LogUAC` reporting a direct
launch and possible false-positive kicks: game-internal checking is present,
and this run must not be described as disabling all anti-cheat logic. No such
check was patched. Production endpoints named by the game remained unreachable
through the isolated network namespace. No credentials were supplied.
