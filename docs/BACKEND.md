# Native private backend

The current backend is original C++/Qt code, separate from Wine, the Windows HTTP
adapter and Project Reboot's gameplay server. It binds IPv4 loopback only. It does
not connect to Epic, accept real Epic accounts, distribute game files, implement
matchmaking, or disable client authentication/certificate checks.

Verified so far: local password authentication over real HTTP, token verification,
expiry, refresh rotation, revocation, and rejection of invalid credentials. The
real 12.50 client also reaches this service and obtains an **application** token.
That token has no player identity. Player login from Fortnite is not yet verified.
Other backend contracts, XMPP, profiles and gameplay remain incomplete.
The real client accepts the private version-check response; its next observed
failure is a missing hotfix/cloud-storage service response, not network transport.

## Setup and isolated game test

```sh
./build.sh
bash helpers/windows/build.sh
./build/fortnite-backend init
./build/fortnite-linux launch 12.50 --offline-probe --launcher-context-probe \
  --proton experimental-20260924 --timeout 60 \
  --http-probe-helper build/windows/fol-module-bootstrap.exe \
  --http-module build/windows/fol-http-local-router.dll --private-backend
```

Use your imported runtime ID. Init creates a random local password and a salted
PBKDF2-SHA256 identity under `$XDG_CONFIG_HOME/fortnite-on-linux/backend/` (or the
normal ~/.config fallback). `identity.json` contains the verifier;
`local-login.json` contains only the generated `player@localhost` development
login. Both are owner-read/write files, and init refuses to overwrite them. Do not
commit or share the login file. No real Epic credentials are needed or requested.

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
experiment only handles a bounded observation window/first eight calls; it is not
yet a persistent private launch workflow. Authentication, certificates, security
checks, exit behavior and game files are not patched.

## Service scope

- Public local application grant: `client_credentials`, no player account claim.
- Player grant: password for the configured @localhost identity only.
- Random expiring access/refresh tokens, refresh rotation, verified bearer access
  and session revocation. Unknown/exchange grants are rejected.
- Private account lookup requires a player token, not an application token.
- Health and local version-check policy; no game updates are distributed.
- Unsupported services fail with explicit errors, never silent success.

Transport bounds headers/body/connections/timeouts, requires a loopback Host header,
rejects duplicate headers and unsupported chunking/pipelining, and sends no-store
responses. Logs contain fixed categories/status/session/PID, not raw requests,
passwords, tokens or response bodies. This is an experimental local service, not
a production authentication service or a reviewed Internet-facing server.

Upstream Reboot Launcher auth_backend/structure/user.js and version.js were read
for interface shapes. Their hardcoded tokens, permissive password behavior, global
account state and request-body logging were not copied. All implementation here is
original; GPL source was not imported.
