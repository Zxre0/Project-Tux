# Read-only HTTP interface discovery

This optional, original C helper is a Windows PE component. The launcher remains
native Linux. It is **not** a backend, an authentication module or a routing DLL.
Its purpose is to collect the missing 12.50 interface evidence needed before a
scoped local-backend adapter can be designed. Candidate discovery is not login.

On the tested CachyOS host, existing Clang/LLD plus Wine's Windows and MSVCRT
headers/import libraries build it without installing a Windows SDK:

```sh
bash helpers/windows/build.sh
cmake --build build --target windows-probe-runner
./build/windows-probe-runner build/windows/fol-http-probe.exe build/windows/fol-http-fixture.exe
```

The manual test requires an initialized, idle launcher-owned `runtime-smoke` Wine
prefix. It runs an asset-free PE fixture inside a user/network namespace and uses
the native supervisor. It is not automatically executed by CI. Header/library
paths in the optional build script match the tested Wine package layout; other
distributions may require adjustment. Generated artifacts stay in ignored build/.

The helper accepts no PID. It creates only the named Shipping executable (or the
explicit synthetic fixture), assigns its initially suspended child to a Windows
kill-on-close job, then resumes it. This does not create/suspend an EAC helper.
It waits at most ten three-second intervals for interface observations; the outer
native supervisor also enforces a deadline and scoped cleanup. Only that child
is queried/read. It does not write remote memory, inject code, disable certificate
validation, change exit behavior, alter game files or contact a service.

It searches a bounded readable in-memory image for the known HTTP stat marker,
executable RIP-relative references, exception-directory function bounds and pointer
references. By default it reports RVAs/counts only, not raw memory, request data or credentials.
Unreadable/guard regions are skipped. Missing or ambiguous evidence is not a license
to guess a hook. Reported pointers do not by themselves prove a vtable or callable
Unreal ABI. Runtime function tables and allocator/virtual-method semantics need
separate validation before any future adapter can call game methods.

For manual ABI inspection only, `FOL_HTTP_PROBE_CODE=1` records a bounded 128-byte
executable-code window at each observed table function (at most 129 entries).
This explicit diagnostic is recorded in the launch command and never captures
request objects, heap data, credentials or a full image. Code observations stay in
the owner-only local `http-probe.jsonl`; do not commit them or distribute game code.
They are observations for disassembly, not validated signatures or method names.

The optional launcher path is gated by the embedded build profile:

```sh
./build/fortnite-linux launch 12.50 --offline-probe --launcher-context-probe \
  --proton experimental-20260924 --timeout 60 \
  --http-probe-helper build/windows/fol-http-probe.exe
```

Only pass the trusted helper built from this source. The launcher records its
SHA-256, checks PE architecture/type, requires an initialized standard Z drive
mapping, preserves anti-cheat observation and excludes simultaneous backend
experiments. A hash receipt is not a security audit of arbitrary executables.
Helper exit status is not game exit status, and neither establishes login.

The helper requires `FOL_HTTP_PROBE_LOG` (a Windows path) and `FOL_SESSION_ID`.
It creates a new report rather than overwriting one. Native callers supply an
owner-only session directory. A bounded, session-bound report with an explicit
completion record is mandatory: Proton's wrapper can return zero without
forwarding a Windows helper's stdout or exit status. The launcher reports missing,
truncated, mismatched and failed discovery receipts as errors.

Real 12.50 probe `2026-09-29_19-39-02_27490309` observed one marker reference with
exception-directory function bounds and one pointer reference after executable
initialization. This is candidate evidence only; the helper then closed its child
job, before engine logging. No backend request or login was performed. See
docs/PROGRESS.md for the failed stdout-only predecessor and exact results.
