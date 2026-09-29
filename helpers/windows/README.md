# Windows HTTP interface diagnostics

The default `fol-http-probe.exe` is read-only. The debugger observer and the
explicit module-bootstrap variant below are separate experiments, not normal launch.

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

## Request observer experiment (not a working game integration)

`fol-http-observer.exe` is a separately compiled variant using the Windows debugger
API and one hardware execution breakpoint. It does not patch game code, inject a
module, rewrite URLs, change authentication/certificates, or suppress unrelated
exceptions. Only its own child may be observed. Thread ownership is checked during
cleanup; original debug-register settings are restored before detaching. Native
launch requires the exact supplied 12.50 executable SHA-256, with the candidate
layout taken from the embedded profile, not user-supplied offsets.

The synthetic fixture proves eight calls can be caught, with matching object layout
and fixed origin/endpoint categories. URL text, queries, headers and bodies are not
reported; the fixture includes a fake query secret to check this. This does not
establish that the same observer works on Fortnite.

Real game tests fail after debugger attachment, even with no breakpoint installed.
The same helper without debugger attachment reaches logged manual sign-in. Do not
retry the identical failing debugger path or disable checks to make it work. The
next integration approach follows upstream's separate Windows module lifecycle.
This observer remains an explicit diagnostic, never part of normal launch.

Diagnostic controls (recorded in launch commands):

- `FOL_HTTP_OBSERVER_DELAY_MS=10000`: delay attachment; range0–30000ms.
- `FOL_HTTP_OBSERVER_MODE=hold-only`: no debugger; observe child liveness for20s.
- `FOL_HTTP_OBSERVER_MODE=attach-only`: attach but install no breakpoint.

Control runs deliberately return an observation failure, even if their control
receipt is complete. They are not routing, backend or login success.

## Owned-child module lifecycle

`fol-module-bootstrap.exe` loads the original `fol-http-adapter.dll` into only the
child it created. This variant writes a DLL pathname to a fresh RW allocation and
calls Windows LoadLibraryW on an owned-child thread. It resolves forwarded loader
exports against the child's actual module base and checks the runtime code match.
It does not attach a debugger or patch game instructions, authentication or checks.
The DLL currently only writes a session/child-bound readiness receipt from a worker
thread; it does not inspect requests, hook methods, route traffic or log in.

The build generates an adjacent version/role/SHA-256 manifest. Native launch checks
PE x64 DLL structure, the exact 12.50 Shipping hash, the manifest and the receipt.
A matching manifest is not a claim of runtime compatibility. Supplying a DLL remains
an explicit trust decision; only use the project-built diagnostic artifact.

```sh
bash helpers/windows/build.sh
./build/windows-probe-runner build/windows/fol-module-bootstrap.exe \
  build/windows/fol-http-fixture.exe build/windows/fol-http-adapter.dll
./build/fortnite-linux launch 12.50 --offline-probe --launcher-context-probe \
  --proton experimental-20260924 --timeout 60 \
  --http-probe-helper build/windows/fol-module-bootstrap.exe \
  --http-module build/windows/fol-http-adapter.dll
```

The fixture and real 12.50 run both verified module presence, its in-child receipt,
and 20 seconds of child liveness. The real game retained logged manual sign-in.
This establishes a module lifecycle, not a private-backend connection or login.

## In-process request observation

`fol-http-request-observer.dll` is a separate artifact with manifest role
`http-request-observer`. Select it using the same bootstrap and `--http-module`
option. It validates the hash-gated profile layout, PE section roles, memory pages
and expected function pointer, atomically replaces that data pointer, and forwards
every invocation unchanged. No game instructions, URLs, authentication or certificate
checks are patched. It restores the pointer and original page permission after20s;
the bootstrap waits25s and the module remains mapped until owned-child shutdown.
No unloading race with late callbacks is introduced.

The observer checks the request vptr and bounded FString descriptor, retaining at
most eight fixed origin/endpoint categories. It never writes a URL, query, header,
body or credential to its receipt. Raw URL scratch space is cleared after inspection.
The session/child-bound `http-module.json.requests` must show installation,
restoration and valid observations; readiness or wrapper exit0 alone does not pass.
Fixture queries contain a fake secret and the manual runner checks it is absent.
The asset-free fixture also checks the forwarded return value.

Wine's fixture data page is PAGE_WRITECOPY. The real 12.50 Proton mapping reports
PAGE_EXECUTE_WRITECOPY for the checked .rdata table as well as code. Validation
therefore separately requires a non-executable PE data section for the pointer
and an executable section for its original target. Permission changes preserve
existing execution permission and restore the original flags. Guard/no-access
pages, unexpected pointers and executable PE-section targets for the table fail
closed. Observing requests is still not backend routing or authentication.

## Explicit local router

`fol-http-local-router.dll` adds the separately profile-bound setter experiment.
Unlike the observer, it rewrites allowlisted private-service URLs to fixed loopback
HTTP and reads back the resulting FString to verify the copy. It never modifies
authentication, certificate checks or executable instructions. Native manifest
role is `http-local-router`; it cannot silently replace the observer artifact.
See [the backend guide](../../docs/BACKEND.md) for verified scope, strict URL
selection, isolated service setup and the current eight-call/20s limitation.
