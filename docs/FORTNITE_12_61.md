# Fortnite 12.61 status

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

Current blockers: missing legitimate 12.61 files; no verified unmodified private
execution path; no reviewed compatible module/backend environment. Client startup,
private join, hosting, gameplay, fullscreen, input and audio are all unverified.
