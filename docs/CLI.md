# fortnite-linux CLI reference notes

Detailed behaviour of the native CLI commands listed in the [README](../README.md#cli).
These commands are the diagnostic/prefix-management layer; for playing a match use
[MATCH.md](MATCH.md).

Registration supports multiple IDs (`add PATH --id custom-name`). Detection reads
ASCII/UTF-16 release metadata and checks PE architecture, section bounds, content
folders and readable nonempty paks. It does **not** prove asset authenticity or
full integrity. Conflicting/unknown metadata is rejected. No unofficial downloads.

Optional integrity manifests contain a `[files]` table mapping quoted relative
file paths to SHA-256 strings. Only listed files are checked; manifest provenance
is not authenticated. Paths escaping the installation, including symlink escapes,
are rejected. The same `--manifest` option on `module` expects `fortnite_version`
and `sha256` fields instead. See [12.50 validation](docs/FORTNITE_12_50.md#optional-content-integrity-checks).

`manifest PATH` independently reads supported legacy binary Epic manifests.
Add `--installation ROOT` to check listed file presence/sizes, and `--hash-files`
to stream SHA-1 verification (a full installation can take several minutes).
This is read-only and never executes the manifest's launcher or downloads files.
Matches prove consistency with the supplied local manifest, not authenticity.
See [manifest verification](docs/MANIFESTS.md) for scope and format limits.

`imports PE_PATH` inventories immediate/delayed DLL names and candidate files in
the executable directory and repeated `--dll-dir` directories. It does not load
DLLs, change Wine search paths, or equate missing optional imports with failure.
See [dependency diagnostics](docs/DEPENDENCIES.md).

Normal `launch` and `host` currently report a blocker. `--offline-probe` is only a
network-isolated startup experiment using installed Wine or an imported Proton script, not working
offline gameplay. It requires unprivileged Linux user/network namespaces and an
initialized dedicated Wine prefix, or imported Proton with separate managed
compatibility data. It never starts the EAC bootstrap intentionally.
Process-name monitoring is sampled; absence from snapshots is not proof that EAC
never loaded. See [EAC findings](docs/EAC.md).

`--trace-startup` adds Wine message-box, debug-string, process, shell and registry
traces in the dedicated prefix. Treat trace logs as private diagnostic material.
`--engine-stdout` separately requests engine stdout logging and line flushing;
Shipping builds may ignore these arguments. They did not explain the tested
12.50 exit 1. Neither option changes authentication or anti-cheat requirements.
Commands explicitly record inherited WINEDLLPATH, LD_LIBRARY_PATH and WINEARCH
when set, without dumping the entire environment. See the
[12.50 runtime comparison](docs/RUNTIME_COMPARISON.md) for the separate bundled-Wine
experiment and its verified dependency fix, which did not resolve exit 1.
The latest registry trace shows a failed Epic launcher-registration lookup before
shutdown; this is a lead, not proof of an authentication or anti-cheat requirement.

The core detects Steam-local Proton installations and tests imported runtime copies.
`launch --proton ID --offline-probe` runs an imported Proton script on the host;
it is not a Steam Linux Runtime/UMU container launch. Experimental and Hotfix
both started supplied 12.50 Shipping and reproduced exit 1. DXVK/VKD3D game rendering validation,
runtime downloads, complete prefix lifecycle,
private backend/server integration, server browser, dev-loop and GUI remain
unimplemented. WineD3D is the unchanged Wine baseline. Gamescope is optional and
currently only detected. No production services or router configuration are used.

Prefix recovery uses private XDG data storage in `prefix-backups/ID/TOKEN/`.
Copy backups preserve symlinks without following Wine's links into the host.
Archive/reset retain the previous prefix; restore consumes the selected backup
by renaming it into place and refuses to overwrite an existing prefix. Incomplete
backups are retained for inspection but cannot be restored automatically. Stop all
prefix users first; launcher locks cannot prevent an independently started Wine.
No permanent prefix deletion or automatic backup pruning is implemented.

`runtime test ID` runs Wine's built-in cmd.exe version command and writes a unique
execution receipt in an offline namespace, then waits for that namespace's Wine
server. This tests Windows command execution
and clean shutdown only; it does not test D3D rendering or Fortnite compatibility.

`runtime import-proton DIRECTORY --id NAME` copies a trusted, already-installed
Proton runtime into project XDG data. It does not download runtimes or modify the
source. `runtime test-proton PREFIX_ID --proton NAME` uses separate
`proton-prefixes/PREFIX_ID/pfx` compatibility data, no game files and no Steam
account. A unique Windows-written receipt and clean server shutdown are required
for success; exit zero alone is insufficient. Custom `user_settings.py` is rejected,
and inherited Steam/Proton/Wine overrides are removed from this baseline test.
This supports the tested `files/bin/wine` layout, not every Proton/UMU release.
Wine prefix backup commands do not currently manage Proton compatibility data.
Proton game probes use the same ownership checks and hold prefix/runtime locks
through execution. A prefix cannot silently switch Proton IDs. Existing Steam,
Proton, Wine and media override variables are cleared and recorded as unsets in
the command; no real Steam installation or account configuration is used.
`--print-command` does not create Proton compatibility data. A discovered
`steam-runtime-launcher-interface-0` is rejected for these host probes because
handing execution to an external service would invalidate the namespace assumption.
The script's `PROTON_LOG=0` keeps its separate log redirection off, while explicit
`WINEDEBUG` enables module/registry traces in supervised `wine.log`. Thus this
setting does **not** disable Wine diagnostics. Runtime-test logging is unchanged.
The reported exit code is the Proton launcher status; logs must establish the
game's own execution/exit. A zero wrapper exit is not gameplay success.
Both runtime tests handle SIGINT/SIGTERM and bounded timeout with session-scoped
cleanup and process-tree logs. EAC-name observation fails the test and stops its
processes. SIGKILL or machine power loss cannot be handled by userspace cleanup.

Configuration, data, cache and logs follow XDG conventions under
`fortnite-on-linux`. Every launch request records a unique session under
`~/.local/state/fortnite-on-linux/logs/sessions/`, including blocked preflight.
After successful process cleanup, newly modified prefix-local FortniteGame.log
files are copied into the session as fortnite.log (additional users receive numbered
files). Collection skips stale, unsafe or over-64-MiB files and reports its result;
freshness is a timestamp check, not log-content attestation. Raw `crashed` is a
QProcess exit-status flag: inspect `stop_requested` and `crash_diagnosis` before
treating a bounded probe's SIGTERM exit as a spontaneous game crash.

Captured logs include structured engine/backend/sign-in observations with source
line numbers. To inspect an existing log without launching anything:

```sh
fortnite-linux logs analyze /path/to/session/fortnite.log
```

This reports logged stages, not proof of rendering, login or gameplay. Backend
configuration rejection is reported separately from the runtime's exit status.

`--local-backend-probe` is a gated 12.50 discovery experiment, not a working server
redirect. The initial MCPCONFIG option did not change the selected backend; the
current experiment tests the separate built-in epicapp Localhost selector. It
requires both `--offline-probe` and `--launcher-context-probe`. No backend service
is automatically started. See [networking evidence](docs/NETWORKING.md).
The separate `--backend-probe-method service-ini` experiment tests built-in
loopback service-address configuration instead of the Localhost selector. It
requires the same opt-ins and does not imply successful routing or login.
The CLI prints JSON and returns nonzero on validation or launch failure.
