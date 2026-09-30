# Native private backend

**Update:** an optional [LawinServerV2 provider](../integrations/lawin/README.md)
now passes the supplied12.50 client's QueryKeychain stage and reaches its
`Completed` login stage in two actual runs. The native implementation below
remains the default and still rejects at keychain. A startup-error dialog and
profile-token verification failure remain with Lawin; no stable menu/gameplay
success is claimed. Select the provider explicitly with `--lawin-config`.
The optional provider now also authenticates real-game XMPP and creates a local
party accepted by the game as Active; see the integration README for evidence and
limitations. The native default backend's historical limitations below are unchanged.

The current backend is original C++/Qt code, separate from Wine, the Windows HTTP
adapter and Project Reboot's gameplay server. It binds IPv4 loopback only. It does
not connect to Epic, accept real Epic accounts, distribute game files, implement
matchmaking, or disable client authentication/certificate checks.

Verified so far: local password authentication over real HTTP, token verification,
expiry, refresh rotation, revocation, and rejection of invalid credentials. The
real 12.50 client also reaches this service and obtains an **application** token.
That token has no player identity. With `--development-login`, the game submitted
the generated local password and received a **player** token. Its subsequent
kill-other-sessions request exposed a backend bug: the HTTP query was discarded
and the current token was revoked. The fix preserves the caller's token and
revokes other local player sessions, including refresh tokens. It passes service
tests and now passes the actual account-login stage: the game logs
`Successfully logged in user` and `LoggedIn`. The actual game also accepts bulk
account lookup, the local PC platform policy (text/plain boolean), and private
Lightswitch service status. An empty configured-feature list and authenticated
user-cloud index now advance it to QueryProfile. The game accepts original empty
common_public/common_core profiles and reaches QueryKeychain, where it rejects
the backend's empty key list. A profile-token verification failure was also
observed in an earlier run. No stable logged-in menu or gameplay has been
verified; no Epic entitlement or client-protection override is implemented.
Other backend contracts, XMPP, profiles and gameplay remain incomplete.
The real client accepts the private version-check and empty cloud-storage index.
**Successful player login in the game is not yet verified.**

## Setup and isolated game test

```sh
./build.sh
bash helpers/windows/build.sh
./build/fortnite-backend init
./build/fortnite-linux launch 12.50 --offline-probe --launcher-context-probe \
  --proton experimental-20260924 --timeout 120 \
  --http-probe-helper build/windows/fol-module-bootstrap.exe \
  --http-module build/windows/fol-http-local-router.dll --private-backend \
  --development-login
```

Use your imported runtime ID. Init creates a random local password and a salted
PBKDF2-SHA256 identity under `$XDG_CONFIG_HOME/fortnite-on-linux/backend/` (or the
normal ~/.config fallback). `identity.json` contains the verifier;
`local-login.json` contains only the generated `player@localhost` development
login. Both are owner-read/write files, and init refuses to overwrite them. Do not
commit or share the login file. No real Epic credentials are needed or requested.
Run init only once. The optional development-login flag passes ordinary login
arguments inside the owned-child helper, not in the printed launcher command.
It rejects verbose startup/argument tracing. Session game-log copies redact the
known generated password; original logs in the dedicated prefix may still contain
it and must not be shared. Local process arguments can also expose it to the same
user. This is a disposable local development account, not an Epic account.

The native launcher starts `fortnite-backend run` and Proton in the same new
user/network namespace. Before unshare it records its namespace device/inode; the
backend requires a different current identity before enabling that namespace's
loopback. Reading PID1's namespace inside the child is not reliable (permission
denied on the tested host). The receipt is an assertion from the trusted launcher,
not a defense against a caller deliberately lying to the standalone CLI.
No host interface, default route, router mapping or firewall is changed.

The explicit router DLL is executable-hash/layout/manifest gated. It allows only
selected account/Fortnite/lightswitch/content/launcher API paths on matching
`.ol.epicgames.com` or `.ol.epicgames.net` hosts, and changes the URL through the
validated game setter to `http://127.0.0.1:3551`. It verifies the resulting string.
Unknown services remain unreachable through namespace isolation. This first
experiment handles up to 4096 calls during a bounded observation window, while
recording only the first eight request categories. The launcher computes the
window from the timeout (85 seconds with the command above), capped at five
minutes. It is not yet a persistent private launch workflow. Authentication, certificates, security
checks, exit behavior and game files are not patched.

## Service scope

- Public local application grant: `client_credentials`, no player account claim.
- Player grant: password for the configured @localhost identity only.
- Random expiring access/refresh tokens, refresh rotation, verified bearer access
  and session revocation. Unknown/exchange grants are rejected.
- Private account lookup requires a player token, not an application token.
- Empty external-auth bindings for the local account; no external credentials.
- Authenticated empty system-cloud index: no private hotfix files are configured.
- Authenticated empty user-cloud index and optional-feature list. Uploads and
  cloud-file persistence are not implemented; no fake settings files are served.
- Authenticated local account lookup/list and PC platform policy.
- Private backend availability, without catalogue overrides or download claims;
  this is not proof that a gameplay server is ready.
- Read-only, in-memory common_public/common_core profiles with empty inventory.
  Snapshots are stable during one backend lifetime; no edits/persistence yet.
- Empty asset keychain only. The real client rejects it with UnableToParseJson;
  this remains unresolved. No asset keys are extracted, downloaded or included.
- Health and local version-check policy; no game updates are distributed.
- Unsupported services fail with explicit errors, never silent success.

Transport bounds headers/body/connections/timeouts, requires a loopback Host header,
rejects duplicate headers and unsupported chunking/pipelining, and sends no-store
responses. Logs contain fixed categories/status/session/PID and, on the platform
policy route, a validated short platform tag (1-16 ASCII characters), not raw requests,
passwords, tokens or response bodies. This is an experimental local service, not
a production authentication service or a reviewed Internet-facing server.
Unimplemented Fortnite routes also report a fixed-word path template: account
identifiers and unknown segments are replaced, and query strings are excluded.

Upstream Reboot Launcher auth_backend/structure/user.js, version.js and
cloudstorage.js were read
for interface shapes. Their hardcoded tokens, permissive password behavior, global
account state and request-body logging were not copied. All implementation here is
original; GPL source was not imported.

## Reference comparison: current keychain blocker

At Reboot Launcher revision `c6f82298b167ef2076bb59dc621f13dc5bd8d390`,
`auth_backend/structure/storefront.js` serves
`/fortnite/api/storefront/v2/keychain` with Express `res.json(keychain)`, where
the value comes from its bundled `responses/keychain.json`. Only the root shape
was inspected: an array of strings. The data was not copied or served here.
A repository-wide text search of Project Reboot 3.0 revision
`3f6417ee6b07063bb6f83902ad3971f998888bab` found no `keychain` or
`UnableToParseJson` matches. This contract belongs to the companion backend,
not an identified gameplay-server workaround.

An independent [API declaration](https://gist.github.com/Amrsatrio/20d2174583354ae4f0a24cf63764049f)
also describes a string-array response. These references establish the broad
shape, not a 12.50-compatible minimal response or authorization to redistribute
asset keys. No Epic requests, key extraction, placeholder keys or client-check
changes are part of this investigation.

Session `2026-09-29_22-21-13_39cd6d5a` rejects compact `[]`; session
`2026-09-29_22-24-56_1dabb94e` rejects the same empty array with whitespace.
Both log `UnableToParseJson` with HTTP200 and abort at QueryKeychain/result37.
The exact parser requirement remains unproven. A successful backend account
authentication is not a completed game login, and the independent startup-error
dialog still needs resolution. There is no genuine successful-login screenshot.

### Broader GitHub comparison (2026-09-29)

Read route source at pinned revisions, without installing/running upstream
backends or downloading their keychain datasets:

| Reference | Keychain handler behavior | What it establishes |
| --- | --- | --- |
| [LawinServer](https://github.com/Lawin0129/LawinServer/blob/7f0f26d7a772c6122c42b1783fd75f497e86d3a9/structure/storefront.js#L24) | Express JSON response from bundled file | Same route shape as Reboot Launcher; no version branch in this handler |
| [LawinServerV2](https://github.com/Lawin0129/LawinServerV2/blob/2205696e4e1f828b4f6457233901373c07f3e180/routes/storefront.js#L52) | Express JSON response from bundled file | No empty-array fallback or 12.50-specific handling here |
| [Reload Backend](https://github.com/Project-Reload/Reload-Backend/blob/a0a35e799231e49749809902cc7839348dbc0896/routes/storefront.js#L59) | Same pattern, with route logging | Not independent evidence of parser behavior; shares Lawin lineage |
| [Aphrodite](https://github.com/simplyzetax/Aphrodite/blob/0726ce9e91e883d5930342ceef4610d9deaef008/src/routes/storefront.ts#L10) | Reads static JSON through Bun and returns it | Another framework, but no version-specific shape conversion |
| [NeoniteV2](https://github.com/NeoniteDev/NeoniteV2/blob/8a60e5c5ca7cf0d37cd36450f19f2e5c02439536/managers/api.js#L1025) | Remote fetch, falling back to a one-string array | A new nonempty-array hypothesis, not proof that empty arrays or arbitrary strings work |

No upstream key value, production credential, permissive authentication handler,
profile-verification shortcut or certificate/anti-cheat workaround is reused.
The remote fetch was not executed. Lawin V1/V2 identify GPL-3.0 licenses;
Aphrodite has no license reported by GitHub, and Neonite's license was not recognized
as a standard SPDX license. This comparison records behavior only, not copied code.
Issue searches for keychain in Lawin V1/V2 and Neonite returned no matches.

The resulting diagnostic experiment changes only the local response from `[]`
to `[""]`: one empty string, no asset key. It is intentionally not a production
keychain or a proposed protection workaround. Acceptance or rejection can narrow
the array-length hypothesis without granting access to encrypted content. The
temporary response must be removed after the experiment; do not label a malformed
fixture as verified keychain support.

**Result:** session `2026-09-29_22-43-29_10e19784` served `[""]` with HTTP200.
The real client still reported `UnableToParseJson`, then QueryKeychain/result37
(game-log lines2807–2809). This rules out a fix based only on making the array
nonempty. It does not distinguish entry validation from response handling, or
prove a need for real asset keys. Reverted the temporary response immediately.
Owned-process cleanup completed; no EAC observed during sampled monitoring.

## Verified supplied-12.50 Lawin lobby (2026-09-30)

This supersedes the earlier unsuccessful-login experiments above. The isolated
Lawin V2 integration now reaches the Chapter2 Season2 lobby with the generated
Local Player account. Battle Royale selection and Locker/Play navigation were
verified using ordinary Windows input. There is no loaded outfit in the test
profile, so the player character is absent. Matchmaking, private game-server
joining and 12.61 support are **not** established by this result.

Prerequisites remain the registered legitimate12.50 installation, prepared owned
Experimental prefix/runtime, checked locally built modules and isolated Lawin
deployment described in [the integration guide](../integrations/lawin/README.md).
From the repository root, the tested command is:

```sh
env -u OPENSSL_ia32cap ./build/fortnite-linux launch 12.50 \
  --offline-probe --launcher-context-probe \
  --proton experimental-20260924 --timeout 360 \
  --http-probe-helper build/windows/fol-module-bootstrap.exe \
  --http-module build/windows/fol-http-local-router.dll \
  --private-backend --development-login \
  --lawin-config /path/to/owned/backend/lawin.json \
  --launcher-identity-probe --reboot-bootstrap-context-probe
```

This is an explicit spoofed bootstrap-context experiment: see [EAC scope](EAC.md).
The command-line contract declares an EAC origin but does not launch EAC. It must
remain network isolated. No Epic account is used. A suspended FortniteLauncher
is not needed in the tested path; omit `--launcher-presence-probe`.

For the first-run benchmark prompt, `-SkipInitialBenchmark` did not work in this
Shipping build. The tested prefix-only workaround was to back up
`pfx/drive_c/users/steamuser/AppData/Local/FortniteGame/Saved/Config/WindowsClient/GameUserSettings.ini`
while idle, then change the existing `InitialBenchmarkState=-1` to `1` in its
FortGameUserSettings section. Do not apply this to an unrelated prefix or overwrite
other preferences. It skips the prompt; it does not run or validate a benchmark.

Select Battle Royale, then Play on its expanded game-mode tile. The initial intro
overlay rendered black on this host; its visible bottom-right skip arrow worked.
The next run remembered that choice and reached the lobby without another skip.
Synthetic compositor/XTest events did not visibly operate this game, whereas
Win32 foreground/cursor/SendInput did. The tightly scoped manual diagnostic is
documented in [Windows helper notes](../helpers/windows/README.md). Physical input
and other resolutions still require testing.

The command is a bounded diagnostic session, not a permanent launcher mode.
Module observation is capped at300s; afterwards the helper restores its local URL
interception and closes its owned job, then the native supervisor cleans up the
backend and prefix processes. Helper exit0 does not establish graceful game exit
or gameplay. Session results, real screenshots and the exact tested runtime are
recorded in [PROGRESS.md](PROGRESS.md); screenshots and game assets stay out of Git.
