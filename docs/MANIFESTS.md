# Read-only local manifest verification

```sh
fortnite-linux manifest /path/to/build.manifest
fortnite-linux manifest /path/to/build.manifest --installation /path/to/Fortnite
fortnite-linux manifest /path/to/build.manifest --installation /path/to/Fortnite --hash-files --output integrity.json
```

The first command reports metadata only. The second checks every listed file's
presence and size. The third additionally streams SHA-1 checks in 4 MiB blocks.
Missing files and mismatches return exit code 2; the JSON retains install tags so
an absent optional component can be distinguished from required content. No tags
are silently excluded. Files not listed in the manifest are outside this check.

This does not establish authenticity: a local manifest and its hashes can be
modified together. SHA-1 detects ordinary corruption here, not trusted provenance.
`all_listed_files_match` must be interpreted alongside `file_hashes_checked` and
`scope`. An internal payload hash match alone says nothing about installed files.

The bounded, independent C++ reader supports container versions 17/18, metadata
versions 0/1 with feature levels 13–18, and file-list version 0. The supplied
12.50 file uses container 18 / feature 13; these are different fields, not a
version-detection conflict. Input/decompressed payloads are capped at 64 MiB,
file count at 100,000, string length at 65,536 code units and tags at 1,024 per
file. Encrypted/newer formats, invalid compression, truncation, payload digest
failure, duplicate or unsafe paths and manifest symlink records are rejected.
Installed paths resolving outside the installation are rejected before reading.
Run against a quiescent installation; concurrent mutation is not a supported
verification snapshot. The verifier does not lock or modify game files.

Only metadata needed for identification and file-list integrity is interpreted.
Chunk transport and custom fields are not interpreted or used for downloads.
Manifest executable metadata is reported as evidence only, **never executed**.
The manifest does not select the offline probe executable, arguments, credentials,
modules or anti-cheat behavior. No download, decryption, repair or authentication
capability is included.

Wire-format reference inspected: [Legendary's manifest model](https://github.com/derrod/legendary/blob/master/legendary/models/manifest.py).
No upstream implementation code was copied. The local reader and synthetic test
writer are independent implementations of the needed binary fields.

Asset-free regression tests are a separate `manifest` CTest target and do not
require network sockets, GPU devices, Wine or copyrighted assets.
