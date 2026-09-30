# Archived Fortnite 12.61 investigation

Retired 2026-09-30 at the user's request. The active profile was removed;
12.50 is now the sole supported target. The text below records historical
findings and plans, not current requirements. See [current status](../FORTNITE_12_50.md).

First-class embedded profile: `profiles/fortnite/12.61.toml`. Expected ordinary
shipping executable is `FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe`.
Working directory is its Win64 directory. Upstream launcher metadata associates
CL 13498980 with 12.61; our parser requires actual release metadata and does not
infer 12.61 from the directory name or a lone executable.

Validation requires FortniteGame, Engine, readable content paks, a structurally
valid x64 PE application with bounded section data, and unambiguous release
metadata. It scans shipping, CrashReportClient and Build.version sources in
bounded chunks, including UTF-16LE strings. Detection has passed synthetic-file
tests, not an actual 12.61 installation test. Engine version is reported only when
detected. PE/hash checks are not signature/authenticity verification.

No complete installation was found in Games, mounted volumes, or the wider home
search. An unrelated Alea executable and a 14.60-labelled launcher directory were
found; neither establishes a 12.61 installation and neither was launched or changed.
The machine also has an unmounted Windows NTFS partition. Read-only mounting was
denied by UDisks authorization; files on that volume have not been inspected.

Current blockers: missing legitimate 12.61 files; no verified unmodified private
execution path; no reviewed compatible module/backend environment. Client startup,
private join, hosting, gameplay, fullscreen, input and audio are all unverified.

## Optional content-integrity checks

`verify ID --manifest /path/to/integrity.toml` checks supplied SHA-256 values:

```toml
[files]
"FortniteGame/Content/Paks/example.pak" = "<64 hex characters from a trusted local manifest>"
```

The placeholder is intentionally invalid until replaced. Files are streamed into
the hash function; empty manifests, malformed hashes, traversal and symlinks
resolving outside the installation are rejected. Missing, unreadable, changed
during check and mismatched files produce failure. A matching user-supplied
manifest does not establish authenticity or cover files absent from the manifest.
The launcher never rewrites game files as a response to corruption.

Module compatibility manifests use a separate schema:

```toml
fortnite_version = "12.61"
sha256 = "<64 hex characters for the supplied module>"
```

Even a matching manifest reports runtime compatibility as unverified. No module
loading is performed.
