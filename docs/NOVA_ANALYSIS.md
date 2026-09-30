# Nova game-server research

Inspected [projectnovafn/Nova](https://github.com/projectnovafn/Nova) at
`b6ee4d3121e4d01846f7ce0cc00946237bea3837`, 2026-09-30 UTC. The reference checkout
is outside this repository under XDG cache. It is not modified or integrated.

## Role and compatibility

Nova is a game-server module, not a launcher or a replacement for Lawin's local
account/profile backend. Its README describes the 2022–2025 Project Nova server.
The repository does not document a tested Fortnite 12.50 build. Searching the
whole tree for `12.50`, `12.5`, `12.41`, `12.61` and CL `13137020` found no explicit
target matches. Generic `>= 4.25` branches in KismetMemoryLibrary and CrashPatch
are relevant to the supplied game's engine, but do not establish compatibility.
No Nova runtime test, player join or gameplay has been performed here.

The server has reflected game-mode/player/inventory/building/vehicle/safe-zone
implementations and hooks, alongside Creative/minigame code. Our requested target
remains BR Solo; presence of these implementations is not evidence they work on
12.50. UWorldPatch sets its listen URL port to 7777; it does not establish a
loopback-only bind, firewall policy or authenticated client admission by itself.

## Build target

`Nova/Nova.vcxproj` targets a Windows x64 DynamicLibrary (PE DLL, not ELF), MSVC
v143, Windows SDK 10.0, static runtime, Unicode and C++ latest. Configurations are
Staging and Production. It uses Windows APIs and MSVC syntax, imports MASM build
customizations and embeds developer-specific vcpkg static-library paths on D:
and C:. There is no supplied Linux-native server build script. Crypt32/ws2_32
are declared link dependencies; vendor memcury also requests Dbghelp. Included
UAC server/common code and Unreal-style runtime headers require further review.
The XML imports MASM but contains no MASM source entries; 174 C++ translation
units are listed. Importing an assembler customization is not an assembler input.

We provisioned a separate user-local MSVC/Windows SDK toolchain and subsequently
attempted a Nova research build, described below. No Nova DLL has been produced.
Do not substitute a successful Erbium build for Nova build evidence, or run Nova's
Production configuration unchanged.

## Initialization and backend coupling

- `Engine/Runtime/Launch/Private/Windows/LaunchWindows.cpp`: DllMain starts an
  asynchronous Main, which calls GuardedMain with the process command line.
- `Engine/Runtime/Launch/Private/Launch.cpp`: constructs UNova and calls Init.
- `Engine/Plugins/Runtime/Nova/Private/Nova.cpp`: installs an exception-filter
  detour and resumes the main thread. Loaded initializes global engine flags,
  engine/MCP/crash handling. OnEngineInit loads the memory cache and initializes
  game-mode, world, networking and gameplay hooks. Late initialization is special
  cased for 10.40, not 12.50. Loading into an already-initialized lobby cannot be
  assumed equivalent to its intended early suspended-process initialization.
- `Patches/WorldPatch.cpp`: wraps BeginPlay/listen, Tick and GetNetMode.
- `Patches/FortGameModeAthenaPatch.cpp`: match start/end, pawn spawn, restart,
  safe-zone and voice-token integration.
- Dedicated-session/matchmaking code handles match assignment and restart;
  it does not replace the missing local backend matchmaking implementation.
- `Engine/Runtime/Online/HTTP/Public/Curl/CurlHttp.h` selects Nova production or
  staging HTTPS API endpoints at compile time. Its ProcessRequest implementation
  rewrites selected service URLs there. Neither endpoint is our private backend.
  No requests to Nova services were made during this inspection.

The MCP implementation also changes request identity/permission handling.
ServiceConfigMcp::GetServicePermissionsById returns the first permission entry
without looking up the supplied ID; ProcessRequestAsUser delegates to a client
request. These cannot be adopted as authenticated local-server behavior without
review. We do not copy embedded client identity replacements or security-check
changes. A project name change does not make such behavior acceptable or solve
our technical integration gaps.

## Actual Linux-hosted build experiment

The original Windows research adapter now supports pinned Nova Staging. It parses
only project-local include directories and uses the dedicated compiler prefix.
It does not execute upstream MSBuild events or adopt machine-specific vcpkg paths.
Three absent unused include search directories are reported explicitly.

Using MSVC 19.44.35229 / SDK 10.0.26100.0 on Linux, the listed
`Engine/Runtime/Launch/Private/Windows/LaunchWindows.cpp` compiled successfully.
A full build then stopped at real dependencies:

- VivoxVoiceChat.cpp: fatal C1083, missing `openssl/sha.h` (also uses EVP/HMAC).
- KismetDetoursLibrary.cpp: fatal C1083, missing `detours.h` (Microsoft Detours).

Other early units, including KismetMemoryLibrary.cpp and Nova.cpp, compiled;
this is not complete compilation/linkage. The project also prints an old compiler
version advisory, not a demonstrated fatal compatibility issue. No headers,
cryptographic functions or Detours interfaces were stubbed to hide errors.
Full build requires reviewed Windows-targeted OpenSSL/Detours development inputs;
host ELF libraries are not substitutes. Logs remain in ignored build-nova and
XDG cache. No compiled object was loaded, or Nova service contacted.

## License and decision

The README declares CC0 and separately warns that portions contain Epic-owned
copyrighted material. That warning must be resolved before redistributing any
such portions; do not infer that the declaration grants rights to everything in
the tree. No Nova source, headers, assets or binaries are committed here.

Keep Nova as an architectural reference, especially its separated world,
game-mode, dedicated-session and matchmaking responsibilities. Do not switch
the functioning local lobby backend to Nova or blindly load its DLL. Erbium
remains the alternative with an actually completed Windows research build;
neither candidate is yet a verified 12.50 game server. The next gameplay work
needs a reviewed server initialization path, preserved admission/security checks,
actual UDP readiness and authenticated local matchmaking—not another cosmetic
frontend change.
