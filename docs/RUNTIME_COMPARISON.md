# 12.50 runtime comparison evidence

This is a startup diagnostic, not a performance benchmark or compatibility claim.
The supplied game remains 12.50, not 12.61. All game commands retain a separate
user/network namespace, ordinary Shipping, `-log -d3d11`, anti-cheat observation,
35-second bound and identity-scoped cleanup.

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
