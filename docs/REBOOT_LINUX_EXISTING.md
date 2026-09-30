# Existing Project Reboot Linux build

Inspected upstream commit `3f6417ee6b07063bb6f83902ad3971f998888bab`.
[build-linux.sh](https://github.com/Milxnor/Project-Reboot-3.0/blob/3f6417ee6b07063bb6f83902ad3971f998888bab/build-linux.sh)
is a Bash wrapper invoking MSBuild on `Project Reboot 3.0.vcxproj`, configuration
Release, platform x64, output directory `../Build`. Its comment requires
[msvc-wine](https://github.com/mstorsjo/msvc-wine). It does not call GCC/Clang or
build an ELF game server.

The vcxproj declares DynamicLibrary, MSVC toolset v143, Windows SDK 10.0,
`stdcpplatest`, Unicode, `/utf-8`, whole-program optimization and static MSVC
runtime for Release x64. Output is a Windows x64 PE DLL with Windows ABI. Source
uses Windows APIs, DllMain, MSVC intrinsics, MinHook and an ImGui Win32/DX9 UI.
Linked dependencies include fmt, spdlog, curl, zlib, Oleaut32, Onecore and MinHook;
several are vendor `.lib` files. The script does not provision those dependencies,
validate a game build, initialize a prefix, launch a server, or test Wine runtime
compatibility. The original script was inspected, not executed successfully here;
the separate research build described below does not use MSBuild.

Linux is the build host; Wine can host the Windows compiler. Whether the result
works inside Fortnite under Wine remains a separate, untested runtime question.
No native Linux server implementation was found in this script. We retain this
upstream build concept if a reviewed Windows helper is needed; the new native
launcher uses its own CMake build and does not replace the upstream build system.

Upstream has BSD-3-Clause licensing. No upstream implementation is copied into
the launcher. Any future redistribution must retain required notices. Experimental
compatibility is not inferred from the existence of a Bash build wrapper.

## Subsequent Linux-hosted research build (2026-09-30)

Provisioned a user-local Microsoft compiler/Windows SDK through reviewed
msvc-wine tooling, without changing the game prefix or global packages. Our
[original research adapter](../integrations/windows/README.md) built the pinned
upstream C++ sources using native clang-cl/lld-link with Microsoft's headers,
runtime libraries and upstream vendor `.lib` files. Windows-case include aliases
were needed in the generated tree. fmt's existing older-MSVC compatibility
branch resolved clang's consteval error; an upstream MSVC-accepted narrowing
initializer remains a visible warning. Explicit lowercase SDK libraries resolve
the Linux linker's case-sensitive Dbghelp/Ws2_32 names.

All sources compiled and the Windows x64 PE DLL linked successfully. Historical
artifact `build-reboot/3f6417ee6b07063bb6f83902ad3971f998888bab/ProjectReboot-research.dll`
has SHA256 `f1f2c346a0f66e8e2636dee5f6160a1596d45c2bbf062d387d22c41a5e916698`.
It was **not loaded into Fortnite**; no hosting/gameplay compatibility follows.
The user requested an alternative gameplay project, so this artifact is retained
as build evidence, not selected as the active game-server integration. No upstream
checkout or game file was modified, and generated sources/binaries are not in Git.
