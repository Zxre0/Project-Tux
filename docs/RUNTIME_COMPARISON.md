# 12.50 runtime comparison evidence

This is a startup diagnostic, not a performance benchmark or compatibility claim.
The supplied game remains 12.50, not 12.61. All game commands retain a separate
user/network namespace, ordinary Shipping, `-log -d3d11`, anti-cheat observation,
bounded execution (35 seconds for direct Wine, 45 for Proton-script probes) and
identity-scoped cleanup.

## System Wine baseline

Wine 11.18 uses project prefix `12.50`. It executes the game and reaches engine
shutdown, then exits 1 without a captured reason. See [12.50 evidence](FORTNITE_12_50.md).
Read-only strace observation on 2026-09-29 also ended with exit 1; it did not
identify a missing game asset or late access-denied syscall. A ptrace run can
change timing and debugger visibility and cannot prove the untraced internal path.

## Proton Experimental's bundled Wine, directly invoked

Imported runtime ID: `experimental-20260924`, version file
`1790244902 experimental-11.0-20260924-x86_64`. Its `files/bin/wine --version`
prints `wine-11.0`. No Proton script, Steam client or Steam Linux Runtime was
used for these game probes. Do **not** report this as working Proton integration.

Registered ID/prefix: `12.50-wine-experimental`. Initialization and isolated
cmd.exe receipt test passed before trying the game. Select the imported Wine
explicitly; using the default PATH would select system Wine and could upgrade
this prefix. Automatic runtime pinning is not implemented.

```sh
env PATH="/home/atlas/.local/share/fortnite-on-linux/runtimes/experimental-20260924/files/bin:$PATH" \
  fortnite-linux runtime test 12.50-wine-experimental
env PATH="/home/atlas/.local/share/fortnite-on-linux/runtimes/experimental-20260924/files/bin:$PATH" \
  WINEDLLPATH='/home/atlas/.local/share/fortnite-on-linux/runtimes/experimental-20260924/files/lib/vkd3d:/home/atlas/.local/share/fortnite-on-linux/runtimes/experimental-20260924/files/lib/wine' \
  fortnite-linux launch 12.50-wine-experimental --offline-probe --trace-startup --timeout 35
```

These are machine-specific evidence commands, not installation instructions for
an arbitrary Proton release. Their exact paths are also recorded in session JSON.

| Session, UTC 2026-09-29 | Setup | Result |
| --- | --- | --- |
| `17-48-45_40a1ff46` | Fresh plain-Wine prefix, bundled Wine | Exit 53/c0000135: three VKD3D DLLs missing, cascading wined3d imports |
| `17-49-36_76babe40` | Add WINEDLLPATH matching imported Proton init_wine | Same failure; search path alone insufficient |
| `17-51-10_d18ecc81` | Add the three absent x64 DLLs from that runtime's default prefix | All three load; game reaches engine shutdown and exits 1 |

The Proton script at the imported version initializes DLL search paths and copies
its default prefix; plain `wineboot` did not provision these extra template DLLs.
The experiment added only the missing files to the dedicated experimental prefix
after creating a recoverable backup. No replacements, symlinks into the game,
DLL load overrides, authentication patches or anti-cheat modules were introduced.

Source: imported runtime `files/share/default_pfx/drive_c/windows/system32/`.
Destination: project `prefixes/12.50-wine-experimental/drive_c/windows/system32/`.
The files were independently checked as x64 Wine PE DLLs.

| File | SHA-256 after copy |
| --- | --- |
| `libvkd3d-1.dll` | `0acae0bffef66092a74087f34acc809f956164f4949d670ac15b38943b0f5307` |
| `libvkd3d-shader-1.dll` | `f120492c57cf10997bcded3e69ef9ab372032807a9f397691ab78955bced708d` |
| `libvkd3d-utils-1.dll` | `488605380c8710e7f1f2debf45a14e30ba1cc08a30d0d441d293986516358aeb` |

Backup token: `20260929T175055078-c4e9b257-0a3e-4843-9b27-e559e719e663`.
All three game probes completed cleanup with no anti-cheat observed. Neither
runtime reached verified rendering/menu/gameplay. The fixed dependency failure
does not explain system Wine's exit 1, and this comparison does not rank runtimes.

The launcher now records inherited `WINEDLLPATH`, `LD_LIBRARY_PATH` and `WINEARCH`
explicitly in its command and JSON. Other inherited environment remains outside
that receipt; it is not a hermetic environment snapshot. Never publish full
environment dumps, prefixes, game assets or unreviewed logs as source artifacts.

## Proton-script host probes

The launcher now supports `launch ID --offline-probe --proton IMPORTED_ID`.
Unlike the direct bundled-Wine experiment, this invokes Proton's `run` verb and
uses its prefix template/setup and DLL configuration. Compatibility data lives
in `proton-prefixes/ID/pfx`, not `prefixes/ID`. A runtime-ID ownership marker and
locks prevent accidental cross-runtime reuse and concurrent prefix mutation.
These are **host-script tests, not Steam Linux Runtime container tests**.

```sh
fortnite-linux runtime test-proton 12.50 --proton experimental-20260924
fortnite-linux launch 12.50 --offline-probe --proton experimental-20260924 --trace-startup --timeout 45
fortnite-linux launch 12.50-proton-hotfix --offline-probe --proton hotfix-20260828 --trace-startup --timeout 45
```

The last command uses a separately registered alias for the same game directory.
No files are imported/copied from the game into the repository. Both new prefixes
passed isolated cmd.exe receipt tests first:
`proton-4a8b7dff-d07f-47f2-ac51-e1c347f8f515` (Experimental) and
`proton-0cc0a1ed-f9bc-4550-a538-5281133d4b53` (Hotfix).

| Runtime | Session, UTC 2026-09-29 | Findings |
| --- | --- | --- |
| Experimental 20260924 | `18-03-19_8c56af6f` | Shipping load observed, unsuccessful Epic launcher-key lookup, launcher exit 1, 241 process samples, maximum 13 processes |
| Hotfix 20260828 | `18-05-13_60a3ba6d` | Shipping load, unsuccessful Epic launcher-key lookup and exit 1, 242 process samples, maximum 13 processes |

Both completed before timeout, with no Unix crash, no observed anti-cheat
process/module and no owned processes left. The Experimental Wine trace also
records the game's Wine PID `0134` calling `NtTerminateProcess` with exit code 1
after the null launcher-key handle; wrapper status alone was not used as proof.
Hotfix records the same sequence on game Wine PID `0138`.
No causal claim about that registry lookup is made.

Proton's Wine/Steam compatibility helper and xalia accessibility helper were
observed. `xalia_dxgi.log` identifies **xalia.exe**, not Fortnite, so its successful
Vulkan enumeration is not game-rendering evidence. A Steam-connect warning is
expected with an empty project-owned Steam client directory; it is not established
as the Fortnite exit cause. No real Steam identity or Epic credentials were used.

The launcher sets `PROTON_LOG=0` and explicit `WINEDEBUG`: inspected Proton
`init_session`/`run_proc` source shows this keeps stdout/stderr in the supervisor's
`wine.log` while preserving Wine traces. Separate-file redirection would hide
module loads from that monitor. Tests exercise this environment and module-stop
path. External `steam-runtime-launcher-interface-0` handoff is rejected so the
script cannot silently delegate execution outside the probe's network namespace.

No runtime tested so far has progressed to verified menu or gameplay. This rules
out claiming a working Proton solution and does not prove a protection blocker.
