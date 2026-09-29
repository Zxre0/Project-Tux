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

## Backend bootstrap interfaces

Read-only follow-up at the same pinned commit separates three services:

| Service | Source evidence | Integration consequence |
| --- | --- | --- |
| HTTP backend | auth_backend/index.js uses Express, port 3551; shared backend constants also specify 127.0.0.1:3551 | Native backend process management is separate from Wine/game execution. Node's listen call does not specify a bind address; do not run this unchanged on a private-by-default host. |
| XMPP/matchmaker WebSocket | auth_backend/structure/xmpp.js binds port 80; common backend constants agree | This is not game UDP transport. CloudStorage/DefaultEngine.ini instead advertises port 8080: an actual mismatch to resolve explicitly, not an assumed working default. |
| Gameplay server | auth_backend/Config/config.ini advertises GameServer 127.0.0.1:7777 | Advertisement does not start or validate the Reboot game-server DLL. |

The HTTP backend is a LawinV1 fork packaged for Windows by its build/start scripts;
its implementation is Node/Express, with GPL-3.0 licensing in its own directory.
The inspected package has no real test command. No backend code, response assets,
keys or profiles were copied, and no dependency installation or service execution
was performed. In particular, index.js installs request/response body logging:
an adapter must not blindly log credentials or assume that the upstream service
is production-hardened. The shared launcher health check accepts any HTTP response
from /unknown, which proves reachability, not version/protocol compatibility.

Cloud storage serves its INI files over the backend. Those files therefore do not
by themselves establish initial HTTP routing before the backend is reachable.
The supplied 12.50 executable contains MCPCONFIG=, McpConfigOverride, invalid-backend
diagnostics and localhost strings. These justify a bounded built-in configuration
discovery experiment, not an assumption that a URL or config name is accepted.
No TLS interception, production routing or authentication replacement is needed
to observe how an isolated game parses that option.

## Supplied-build configuration follow-up

The later epicapp Localhost experiment is recognized by 12.50, unlike the earlier
MCPCONFIG experiment, but reports a missing service configuration and rejects the
backend. See FORTNITE_12_50.md for session-specific results. Read-only strings also
identify `OnlineSubsystemMcp.BaseServiceMcp` and `Fortnite.McpConfig`; these names
alone do not specify accepted keys, config-file precedence or a working service
address override. A static reference scan did not resolve a usable configuration
call site. No game instructions, registry, certificate handling or configuration
files were changed, and no protected services were contacted. Do not present a
guessed INI schema as a verified private backend contract.

## Routing dependency source review

Reboot Launcher's GameDll.auth default is named sinum.dll. Independently located
the public [projectnovafn/Sinum source](https://github.com/projectnovafn/Sinum/tree/19fbed919824d1cfba8228158feec28f2fa12403)
and inspected that pinned revision as a reference, not as proof of provenance of
the launcher-bundled DLL. No DLL from either repository was executed.

Windows/README.md and Sinum/Sinum.cpp describe a Windows in-process HTTP request
adapter: it changes the request URL before calling the original ProcessRequest.
Core/Unreal/CurlHttp.cpp contains the URL transformation separately from Core.cpp
and EOS.cpp. Thus the upstream architecture does not rely on the tested INI
overrides. The complete module also includes exit/security-related changes and
an EOS hook, which are not adopted. Any narrow project adapter must omit those
features, preserve certificate checks and normal termination, constrain destinations
to the isolated local service, and never copy real credentials into diagnostic logs.

Source comments verify some nearby signatures on 12.41, not the supplied 12.50;
there is no validated 12.50 address/ABI map yet. Hard-coded Unreal vtable methods,
allocator dependence, pointer discovery and unbounded startup waiting must not be
blindly reused. Native Linux's ABI is not the Windows x64 C++ ABI. The upstream
project uses MSVC v143/Windows SDK, not a Linux ELF target. Local clang-cl/lld-link
and winegcc are present, but a MinGW/Windows SDK header-library set was not found
in the checked standard paths. Subsequent work builds an independent freestanding
PE probe with Clang and Wine's packaged Windows headers/kernel32 library; no
routing-module build or injection has been attempted.

The license is MIT-like **with an additional Project Era usage restriction**, not
unmodified MIT. No source was copied. Preserve the complete notice and restriction
if any reuse is subsequently chosen; do not label it unrestricted MIT.

## Supplied 12.50 runtime interface observations

The original read-only helper first identified the ProcessRequest marker candidate
in session 2026-09-29_19-39-02_27490309. Bounded follow-ups establish why the first
pointer in a contiguous run must not be assumed to be an interface start. Session
2026-09-29_19-44-51_59da246c found a 65-entry code-pointer run beginning at
RVA107847440, with the marker function at run index27. Code references target
indices0,5,41,51,58, consistent with multiple adjacent tables. Run entries0 and5
have deleting-destructor behavior in subsequent code observations; invoking one
as GetURL would be unsafe.

The explicitly requested bounded code observations in
2026-09-29_19-46-43_f25f2108 support a separate logical interface beginning at run
index10 (RVA107847520): its first entry copies the string at object offset104 into
the caller's result, and the entry ten slots later checks request state then assigns
to offset104. The intervening run index17 has deleting-destructor behavior, while
run index18 copies the field at offset120. Thus the getter/setter index hypothesis
is consistent with this particular executable; this is not proof that every
surrounding method is identified, that an actual request object's vptr uses this
base, or that the allocator/calling contract has been exercised successfully.
The marker function would be logical slot17 under this hypothesis.

Code windows are local diagnostic artifacts only and are not committed or included
in launcher summary JSON. Disassembly past short functions includes padding or
unrelated instructions and sometimes ends mid-instruction; those tails are not ABI
evidence. No URL object, authentication payload, certificate state or token was
read. No game function was called or modified. The next integration test must
validate actual request identity and ownership without adopting the complete
upstream module's unrelated patches.

### Rechecked lifecycle after debugger experiment

At the pinned Reboot-Launcher revision, game_start_button.dart:259 loads GameDll.auth
after creating the game; _onLoggedIn at lines439–456 loads console or gameServer
only later. common/lib/src/util/os.dart:286 uses a Windows LoadLibrary/remote-thread
module-loading path, not DebugActiveProcess. Project Reboot3.0 dllmain.cpp:1854–1859
starts its own worker from DLL_PROCESS_ATTACH; it is not the initial backend URL
selection mechanism. No code from those loaders is copied, and their suspended EAC
process creation is specifically excluded from this project's launch chain.

The observational debugger experiment fails on the supplied game (see PROGRESS),
while the identical helper without attachment reaches sign-in. This narrows the
failure to debugger interaction under the tested configuration; it does not prove
an anti-cheat/DRM root cause or invalidate the separate module-based architecture.
Next is an independently implemented, owned-child module lifecycle with fixture
tests, then a non-authentication request observer before any local-only routing.
