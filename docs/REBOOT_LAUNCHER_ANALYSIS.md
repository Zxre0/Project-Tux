# Reboot Launcher reference analysis

Inspected commit `c6f82298b167ef2076bb59dc621f13dc5bd8d390` of
[Auties00/Reboot-Launcher](https://github.com/Auties00/Reboot-Launcher/tree/c6f82298b167ef2076bb59dc621f13dc5bd8d390).

| Area | Source | Finding and Linux design consequence |
| --- | --- | --- |
| Shared domain | common/lib/src/game | Game instance, version, build, module and metadata are separate models; retain those boundaries |
| Platform | common/lib/src/util/os.dart | Win32 FFI, process suspension and injection; Linux process management needs a native implementation |
| Metadata | common/lib/src/game/game_metadata.dart | Uses CrashReportClient Windows property store, then folder-name fallback. Maps CL 13498980 to 12.61. Our parser reads file bytes and does not trust folder labels |
| GUI | gui/lib/src/controller | Separate game, backend, hosting, DLL, browser and settings controllers |
| Launch orchestration | gui/lib/src/button/game_start_button.dart | Creates paused launcher and EAC processes, then game, then authentication module. This is excluded under this project's boundary |
| CLI | cli/lib/main.dart and src/controller/config.dart | WIP Dart interface and JSON versions registry; some command files are empty |
| Backend | common/lib/src/backend; auth_backend | Backend interface distinct from Node service implementation; do not reproduce authentication replacement |
| Browser | common/lib/src/browser; server_browser_backend | Optional WebSocket directory distinct from game transport |
| Host | hosting_controller.dart | Separate instance and server advertisement; do not conflate client, backend and game server |
| Modules | dll_controller.dart | Version-dependent injectable selection; native manager instead validates architecture and supplied compatibility manifest |
| Installations | game_downloader.dart; import UI | Import/discovery can be recreated independently; unofficial game downloads are excluded |
| Persistence | settings_controller.dart; CLI config | User preferences separate from running processes; native project uses XDG TOML |

The normal upstream-created process set includes `FortniteLauncher.exe`,
`FortniteClient-Win64-Shipping_EAC.exe`, and
`FortniteClient-Win64-Shipping.exe`. A suspended EAC process still violates the
requirement that EAC not start. The native launcher does not use that sequence.
The upstream README describes the CLI as Windows VPS-oriented; Dart/Flutter alone
does not make its Win32 shared layer portable.

No repository-wide LICENSE was found in the inspected checkout; auth_backend has
its own license. Therefore architecture is studied and behavior independently
implemented, with no launcher source copied. Controller APIs and private backend
integration are not asserted to work on Linux merely from source inspection.

## Early-startup comparison with the supplied 12.50 build

Re-inspected the pinned source on 2026-09-29 after the user requested an explicit
explanation of how Reboot handles this stage. The client launch path and the
Project Reboot server DLL are different responsibilities:

| Stage | Pinned source | Evidence and decision |
| --- | --- | --- |
| Process setup | [game_start_button.dart](https://github.com/Auties00/Reboot-Launcher/blob/c6f82298b167ef2076bb59dc621f13dc5bd8d390/gui/lib/src/button/game_start_button.dart#L231) | Starts launcher/EAC helpers and Shipping, then requests the authentication module. Excluded from this project's non-EAC/no-authentication-replacement path. |
| Helper suspension | Same file, `_createPausedProcess`, lines 371-400 | Despite the function name, source awaits `startProcess` and then calls `suspend(pid)`. This is not evidence that the executable never ran before suspension; do not port this sequence. |
| Argument construction | [game_metadata.dart](https://github.com/Auties00/Reboot-Launcher/blob/c6f82298b167ef2076bb59dc621f13dc5bd8d390/common/lib/src/game/game_metadata.dart#L201) | Adds production/launcher-origin and authentication-related fields. They are not an independently established private-development contract; no values or tokens are copied. |
| Post-login host setup | game_start_button.dart, `_onLoggedIn`, lines 440-458 | Requests the game-server module only after the login callback. Loading the server DLL is not this launcher's solution to initial launcher discovery. |
| Server DLL initialization | [dllmain.cpp](https://github.com/Milxnor/Project-Reboot-3.0/blob/3f6417ee6b07063bb6f83902ad3971f998888bab/Project%20Reboot%203.0/dllmain.cpp#L906) | Windows DLL attach starts `Main`; initialization discovers version/offsets/addresses inside the game. This is not a native standalone process that supplies launcher prerequisites. |

Source ordering explains the upstream dependency separation; it does not prove
which upstream behavior would change this machine's exit-1 result. No upstream
authentication module was run or ported. No 12.50-specific launcher-discovery fix
was found by the targeted source search.

Two additional compatibility actions occur in `_createGameProcess`: deleting
Aftermath DLLs and setting `OPENSSL_ia32cap`. Aftermath deletion is not adopted:
the captured baseline/Experimental Wine logs contain no matching Aftermath load
or illegal-instruction indication, and the installation's validated files should
not be changed speculatively. Absence of a matching trace is not proof of absence.

The OpenSSL setting is a separate CPU-code-path experiment, not authentication,
SSL certificate/pinning replacement or hardware-identity spoofing. Shipping
contains an `OpenSSL 1.1.1  11 Sep 2018` ASCII string, but this alone does not prove
that it consumes the environment variable. Upstream supplies `~0x20000000`.
[OpenSSL 1.1.1 documentation](https://docs.openssl.org/1.1.1/man3/OPENSSL_ia32cap/)
places SHA extensions in the extended capability word controlled after a colon;
therefore do not describe that upstream value as the documented SHA-extension
mask. A bounded isolated comparison may test its actual effect, without enabling
it as a default or copying the excluded launch chain. Results belong in the
[12.50 diagnostic record](FORTNITE_12_50.md).
