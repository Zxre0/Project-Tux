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
