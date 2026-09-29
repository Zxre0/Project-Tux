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
