# Linux-hosted Windows module research builds

`build_module.py` is an original build adapter, not a module loader. It builds
pinned reference sources into ignored repository `build-PROVIDER/REVISION/`
directories. It does not modify reference checkouts, launch Fortnite, patch game
files or certify module compatibility. Windows artifacts run under Wine only
where needed; the launcher remains native Linux C++/Qt.

## Toolchain

Provision the official Microsoft compiler/SDK separately using a reviewed
[msvc-wine](https://github.com/mstorsjo/msvc-wine) setup. This host used revision
`514f8ea34842cd6d831804d0e9658d3a32870ae1`, Microsoft VS Build Tools 17.14.41,
MSVC 19.44.35229, Windows SDK 10.0.26100.0 and native clang/lld 22.1.8.
The Microsoft license must be accepted separately; these tools are not bundled
or redistributed. Dependency download hash validation was enabled.

Use a dedicated compiler prefix and an offline user/network namespace. The
builder sets `WINEPREFIX=TOOLCHAIN/compiler-prefix` for MSVC, never a game or
default prefix. Follow the toolchain's initialization instructions first.

```sh
unshare --user --map-root-user --net -- python3 integrations/windows/build_module.py \
  --provider erbium --reference /absolute/path/to/Erbium \
  --toolchain /absolute/path/to/msvc-toolchain --jobs 4
```

Erbium defaults to the original Microsoft compiler: native clang-cl rejected its
nonstandard variadic macros and an iterator lookup that MSVC accepts. A narrow
forced-include build compatibility header coalesces the unused header-defined
`_Inits` global with `selectany`; it does not suppress arbitrary duplicate-symbol
errors or alter engine signatures/offsets/hooks. The complete rebuilt DLL linked
successfully. Required DLL imports remain a separate runtime prerequisite.

Historical `--provider reboot` uses native clang-cl with the installed MSVC STL
and SDK. Its source and Windows vendor libraries are retained in the generated
tree. The adapter selects fmt's existing pre-1929 compatibility branch and keeps
MSVC-accepted float-to-int narrowing visible as a warning. Case-insensitive
include aliases are generated only in build output. See
[upstream build findings](../../docs/REBOOT_LINUX_EXISTING.md).

`--source RELATIVE_CPP` compiles one listed translation unit without linking.
`--link-only` is a diagnostic relink of existing objects, not a fresh verification
after source, compiler or flag changes. Perform a full build to verify such
changes. Each compilation and the link have logs; failures are not swallowed.
The final report explicitly marks runtime compatibility unverified and loaded
false. Upstream LICENSE is retained alongside the generated artifact.

No Nova build adapter is enabled: its build and runtime coupling require further
review. No upstream source or generated DLL is tracked in this repository. Do
not distribute or load the unreviewed research artifacts. See
[gameplay status](../../docs/GAMEPLAY_SERVERS.md).
