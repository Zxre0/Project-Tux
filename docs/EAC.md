# EAC/private-development boundary

No legitimate non-EAC private gameplay path has been verified. A supplied 12.50
build is the sole development target. Isolated Lawin lobby and outfit persistence
have been verified; that does not establish a private gameplay path.

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
not a Fortnite execution result. The first launch request failed before execution due
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

The later local-password run `2026-09-29_21-52-24_cc0f8549` reaches authenticated
local-account state in the game log before a missing backend route aborts login.
A real screenshot shows the separate "Fortnite was not started correctly" dialog;
the game still reports its UAC direct-launch warning. No EAC/BattlEye process was
observed by sampled monitoring, but this does not establish the absence of
in-process checks. No check or exit behavior has been patched. The screenshot is
startup-failure evidence, not a successful private-login screenshot.

## Lawin lobby investigation (2026-09-29)

Session `2026-09-29_23-22-29_63f1dcea` now has real-game authenticated XMPP,
successful party creation and Active persistent party state. Nevertheless,
`LogAppES: Display: closing code 1` still precedes the startup dialog and exit.
The profile-token HTTP501 response follows this marker; changing that response
is not an evidence-based fix for the earlier error. The exact AppES condition
has not been established. Do not infer a missing GPU dependency, EAC process or
bad password solely from this generic message.

Read-only strings from the supplied Shipping executable contain both LogAppES
and the displayed error text, plus the profile-token verification endpoint.
No matching AppES implementation was found by source search in the pinned
Project Reboot3.0 or Reboot Launcher C++/header files. Reboot Launcher's Dart
game_start_button explicitly creates paused launcher/EAC processes; its argument
builder declares an EAC launch context. Those dependencies do not constitute a
verified legitimate no-EAC launch path and have not been reproduced here.

An additional [FNLauncher reference](https://github.com/DottoXD/FNLauncher/blob/main/main.go)
likewise starts and suspends launcher/EAC processes. It was inspected only to
classify that dependency, not installed or executed. The [official support note](https://www.epicgames.com/help/c-34254770/c-38015632/a14186258)
associates the generic message with interference or networking problems, but
does not establish its cause for this old build, Wine, or private services.
No protection check, process-presence stand-in, token or exit patch is introduced.

## Current blocking prerequisite

The functional Lawin lobby goal is **not achieved**. Local account authentication,
XMPP, Active party creation and valid empty profile inventories are tested; the
supplied Shipping client still reports its AppES startup rejection and exits.
This persists with both imported Proton runtimes. The exact internal rejection
condition remains unknown; do not equate the message with a proven requirement
for a particular anti-cheat process or pretend that an HTTP501 caused an earlier
client-side marker.

Final read-only checks found no Development/Debug/Test/Server-named executable
in the supplied installation. Both bootstrap configurations target ordinary
Shipping, without a documented private-development entry. Limited static string
reference checks did not identify a usable startup contract; that negative result
does not prove obfuscation or that every possible private path is impossible.
Shipping SHA256 remains the profile-pinned
`34a10af4ddff20b75d889e269a66644d570c3000f13548a5799d3a7ef690190c`.

To resume the blocked launch work, the missing prerequisite is a supported,
authorized private-development launch procedure or suitable client build that
does not require EAC/process spoofing. No such procedure/build was found in the
provided materials. More approval to patch a check is not the missing interface.
Repeating the same launch, returning invented verification success, or claiming
an Active party is a functional lobby would not resolve this prerequisite.

## Opt-in launcher-presence investigation (subsequent scope change)

The user subsequently authorized investigating a spoofed launcher context. The
earlier no-process-stand-in boundary above describes the previous experiment
scope, not proof that another client build is the only possible solution.

`--launcher-presence-probe` now tests one specific upstream difference: the
installation's `FortniteLauncher.exe` is created **suspended**, assigned to the
helper's kill-on-close job and never resumed. The client remains a separate owned
child. This is not authentic Epic launcher authorization. No EAC executable is
selected, suspended or started by this option; EAC/BattlEye monitoring remains
unchanged. Unlike Reboot Launcher's paused-bootstrap flow, this experiment does
not reproduce its additional EAC bootstrap process.

The option requires the hash-gated module bootstrap, isolated launcher context,
local router and private backend. The selected launcher's canonical path, PE
structure and SHA-256 are recorded, and helper receipts identify the separate
Windows process and its cleanup. Inherited arbitrary presence paths are rejected.
No game executable, authentication check, profile-token verifier or exit handler
is patched by the presence option. Production networking remains unavailable.

Tests and actual game results are recorded in PROGRESS.md. Process presence alone
must not be reported as a completed login or a functional lobby.

The separate `--launcher-identity-probe` supplies the 12.50 profile's application
name/environment/locale arguments. It also requires the isolated private backend;
`Prod` is an input label under investigation, not permission for production access.
Both presence alone and presence plus identity still produced AppES closing code1
in actual tests. Neither establishes the underlying rejection condition.

`--reboot-bootstrap-context-probe` additionally tests the public bootstrap
argument group from [pinned Reboot Launcher](https://github.com/Auties00/Reboot-Launcher/blob/c6f82298b167ef2076bb59dc621f13dc5bd8d390/common/lib/src/game/game_metadata.dart).
It explicitly declares an EAC-origin context **without starting EAC**, so this is
spoofing, not authentic launcher authorization or a verified legitimate offline
entry point. The fixed upstream launcher token is not an Epic user's credential.
Network isolation, checked local module, private backend and process/module guards
remain mandatory. No integrity, authentication or exit-handler code is patched by
this option. Do not use it with protected production services.

The first real test with this argument group did not reproduce AppES closing code1
and stayed alive through the145s module-observation window. The game rendered the
initial settings prompt; input/lobby functionality remains unverified. Individual
argument necessity and whether suspended launcher presence is needed remain open.

Subsequent tests reached a rendered Lawin-backed 12.50 lobby **without** suspended
launcher presence. Ordinary Windows input navigated Battle Royale and the
Locker/Play tabs; the earlier AppES marker did not recur through bounded
observation. Keep this distinction explicit: no EAC process/module was observed,
but the command line declares a spoofed bootstrap origin. It is not evidence of
authentic Epic authorization or production compatibility. Full
sessions, screenshots, limitations and cleanup are recorded in PROGRESS.md.
