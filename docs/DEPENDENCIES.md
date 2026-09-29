# Read-only PE dependency diagnostics

```sh
fortnite-linux imports /path/to/FortniteClient-Win64-Shipping.exe \
  --dll-dir /path/to/prefix/drive_c/windows/system32 \
  --dll-dir /path/to/known/third-party/directory \
  --output imports.json
```

The native reader reports direct immediate and delayed import names from x64 PE
images. It inventories the executable directory plus up to 16 additional explicit
directories, case-insensitively. It records each candidate's PE/DLL/architecture
checks, including invalid candidates alongside valid alternatives. API-set
contracts are distinguished from concrete DLL files. The command reports an
inventory, not an executable compatibility verdict; successful inspection returns
zero even if candidates are absent. Malformed/unsupported input returns an error.

No binary is executed or loaded. No environment, search path, game file, prefix or
module is changed. A candidate is not proof that Wine will select it, that all
imported symbols exist, or that its transitive dependencies work. Missing delayed
imports may belong to optional features. APIs mapped internally by Wine need not
have matching files. Dynamic LoadLibrary calls are outside this static inventory.

The independent reader uses Microsoft's [PE format specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
It bounds header/directory reads, translates file-backed RVAs, rejects overlapping
virtual sections and unsafe names, requires descriptor terminators, and supports
RVA-based delayed imports only. PE32/legacy pointer-based delayed descriptors are
not supported. Synthetic tests cover malformed input and candidate reporting.

## Supplied 12.50 findings

Shipping has **50 immediate and 17 delayed** DLL imports. Their names and kinds
match the independent `llvm-readobj --coff-imports` output exactly. With the
executable directory and dedicated Wine prefix system32 directory inventoried,
all immediate names have candidates; nine delayed names require additional
third-party directories:

| Directory relative to installation | Delayed DLLs |
| --- | --- |
| FortniteGame/Binaries/ThirdParty/Discord/Win64 | discord-rpc.dll |
| FortniteGame/Binaries/ThirdParty/SpeechGraphics/Win64 | SG_Com.dll |
| FortniteGame/Binaries/ThirdParty/Nvidia/Win64 | GfeSDK.dll |
| Engine/Binaries/ThirdParty/Windows/XAudio2_9/x64 | XAudio2_9Redist.dll |
| Engine/Binaries/ThirdParty/CEF3/Win64 | libcef.dll |
| Engine/Binaries/ThirdParty/Ogg/Win64/VS2015 | libogg_64.dll |
| Engine/Binaries/ThirdParty/Vorbis/Win64/VS2015 | libvorbis_64.dll, libvorbisfile_64.dll |
| Engine/Binaries/ThirdParty/NVIDIA/NVaftermath/Win64 | GFSDK_Aftermath_Lib.x64.dll |

These directories were discovered in the supplied installation, not downloaded.
Including them yields candidates for all 67 names. It does not emulate how Unreal
or Wine configures DLL search directories at runtime.

An important architecture distinction: the CEF3/Win64 directory's
`d3dcompiler_43.dll` is **x86 PE32**, despite the folder name. The dedicated Wine
prefix's system32 counterpart is **x64 PE32+**. Both were independently checked
using `file`. The inventory flags the former and records the valid alternative.
The corrected real startup log contains no d3dcompiler/libcef load evidence, so
this is **not a proven cause** of the exit. Do not replace or relocate either
file on this evidence alone. The installation's local-manifest hashes match.
