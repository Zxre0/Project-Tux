# EAC/private-development boundary

No legitimate non-EAC private gameplay path has been verified. No game launch has
occurred: the available filesystem has no complete identified Fortnite 12.61 build.

Expected upstream executable names are `FortniteClient-Win64-Shipping.exe`,
`FortniteClient-Win64-Shipping_EAC.exe`, `FortniteLauncher.exe`, and
`CrashReportClient.exe`. EAC detection also recognizes EasyAntiCheat-prefixed
executables/services, `start_protected_game.exe` and `eac_launcher.exe`.
Actual installation-specific inventory remains pending supplied files.

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
`not observed`, not an unconditional `Not Loaded`. Module-load attestation is
not implemented. Preflight without game execution reports `not tested`.

2026-09-29 baseline: no EAC-named processes observed; this is a system snapshot,
not a Fortnite 12.61 result. The first launch request failed before execution due
to missing build registration and has a persisted session result.
