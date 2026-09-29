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
compatibility. The script was inspected, not successfully built here: MSBuild,
MSVC and SDK provisioning are absent.

Linux is the build host; Wine can host the Windows compiler. Whether the result
works inside Fortnite under Wine remains a separate, untested runtime question.
No native Linux server implementation was found in this script. We retain this
upstream build concept if a reviewed Windows helper is needed; the new native
launcher uses its own CMake build and does not replace the upstream build system.

Upstream has BSD-3-Clause licensing. No upstream implementation is copied into
the launcher. Any future redistribution must retain required notices. Experimental
compatibility is not inferred from the existence of a Bash build wrapper.
