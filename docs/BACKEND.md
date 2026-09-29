# Native private backend

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
