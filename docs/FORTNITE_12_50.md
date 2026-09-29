# Fortnite 12.50 diagnostic target

This is a separately supplied diagnostic build, not a substitute for 12.61.
The project's primary compatibility target remains 12.61. No private gameplay,
Reboot module compatibility or non-EAC authorization path is established for 12.50.

Supplied root: `/home/atlas/Documents/Fortnite/++Fortnite+Release-12`.
Ordinary Shipping and CrashReportClient both contain `++Fortnite+Release-12.50`;
Shipping also contains `++Fortnite+Release-12.50-CL-13137020`. The engine version
has not been inferred from Season 12. Structural validation passes for x64 PE,
Engine/FortniteGame folders and readable nonempty pak content; full hashes against
an authenticated manifest have not been checked.

Observed ordinary Shipping SHA-256:
`34a10af4ddff20b75d889e269a66644d570c3000f13548a5799d3a7ef690190c`.
This records the supplied file, not a published authenticity reference.

`profiles/fortnite/12.50.toml` selects only ordinary Shipping with `-log -d3d11`.
Use `launch 12.50 --offline-probe`; the network namespace has no production route
and no host backend connectivity. Dedicated prefix: XDG data `prefixes/12.50`.
Do not invoke Shipping_EAC, Shipping_BE, FortniteLauncher or either anti-cheat
setup/service. Both bootstrap configs point to the ordinary executable, but that
alone does not prove an accepted private/offline execution path.

The first run reached engine shutdown, including LauncherCheck/LauncherPlatform
module teardown, but a launcher cleanup-scope defect prevented a reliable final
result. That defect and possible unrelated-process impact are recorded in
[PROGRESS.md](PROGRESS.md). No game-startup success is claimed from that run.
