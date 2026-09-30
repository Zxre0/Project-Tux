# Isolated LawinServerV2 provider

Experimental integration, explicitly selected; the native backend remains the
default. The native Linux launcher supervises a native Node.js/MongoDB backend
and the Proton game process in one loopback-only network namespace. No host
network interface, firewall, driver, or router setting is changed.

Upstream: https://github.com/Lawin0129/LawinServerV2
Pinned revision: `2205696e4e1f828b4f6457233901373c07f3e180` (GPL-3.0).
The original adapter files in this directory are GPL-3.0-only. Keep upstream
LICENSE and credits in deployments; no upstream source, datasets, game assets,
runtime archives, credentials, databases or session logs are committed here.

## Tested setup

- Native Node.js22.23.3 Linux x64, official release archive SHA256:
  `df450af89261115ef9f9e3830c3eeb2cc9213b63c720b1af623cb5dcbe2e02de`.
- MongoDB7.0.43 Ubuntu22.04 x86_64 official archive, SHA256:
  `cba84b47932ebcdd093de37f2fdbbbd415116d21187ccf9117568726859d6f81`.
- Both extracted under XDG data `fortnite-on-linux/backend-tools/`; no global install.
- CachyOS, Protonexperimental-20260924, supplied Fortnite12.50 CL13137020.

Use official downloads and verify their checksums before extracting. Python3.12+
is needed for preparation (`tarfile` data extraction filter). Node/MongoDB are
needed only for this optional provider, not the native backend. Native bcrypt
compilation also needs a C/C++ build toolchain and Python.

Prepare a fresh deployment (substitute your own absolute paths):

```sh
python3 integrations/lawin/prepare.py \
  --checkout /path/to/pinned/LawinServerV2 \
  --deployment /path/to/xdg-data/fortnite-on-linux/backends/lawin-v2-2205696 \
  --node /path/to/node/bin/node --mongod /path/to/mongodb/bin/mongod \
  --login /path/to/xdg-config/fortnite-on-linux/backend/local-login.json \
  --config /path/to/xdg-config/fortnite-on-linux/backend/lawin.json
```

Preparation archives the pinned committed tree, never modifies the reference
checkout, and refuses existing deployment/config targets. It retains the
upstream npm lockfile. In the deployment's `source` directory, using the pinned
Node runtime on PATH:

```sh
npm ci --ignore-scripts --no-audit --no-fund
npm rebuild bcrypt --build-from-source
```

Dependencies are old; they have **not** been security-audited. Keep the deployment
isolated and do not expose it publicly. Only bcrypt's build is deliberately run;
other dependency lifecycle scripts are not enabled by the install command.

Build the project normally, then append this to the existing isolated launch:

```sh
./build/fortnite-linux launch 12.50 --offline-probe --launcher-context-probe \
  --proton experimental-20260924 --timeout 120 \
  --http-probe-helper build/windows/fol-module-bootstrap.exe \
  --http-module build/windows/fol-http-local-router.dll \
  --private-backend --development-login \
  --lawin-config /absolute/path/to/backend/lawin.json
```

The adapter validates private local credentials, initializes a separate MongoDB
account with bcrypt, and refuses to overwrite an existing mismatching password.
It uses the existing generated @localhost credentials, not Epic credentials.
It creates a small local starter locker rather than importing the upstream full
catalog: eight original default outfits, default pickaxe, glider and dance.
These are metadata references to assets in the user's installation, not Epic
account entitlements. The recognized empty Athena profile is backed up under
the owned deployment's `profile-backups/` (owner-only) before migration. Existing
nonempty lockers and outfit choices are preserved; unfamiliar broken profiles
are rejected rather than overwritten. Other inventories remain empty.
An owner-only deployment receipt is required before starting MongoDB, preventing
accidental use of an unrelated existing database directory.
There is no Discord requirement. A deployment lock prevents two sessions from
using the same database. A forced kill may leave this lock behind; verify no owned
process is still using the database before manually recovering it. The adapter
does not automatically delete stale locks, databases or user registrations.

The service reuses upstream route modules, **not** its index.js entry point:
HTTP binds127.0.0.1:3551; MongoDB binds127.0.0.1:27017 in the private namespace.
Tokens are session-local. An original single-account XMPP adapter now listens on
127.0.0.1:8080 inside the same namespace. It implements authenticated framing,
binding, session, roster and ping; it does not pretend unsupported party rooms or
messages succeeded. SASL requires the exact local access token, a verified JWT
signature/expiry, and the matching existing unbanned local account. An original
DefaultEngine.ini cloud file advertises only this loopback transport; its index
contains hashes of the actual bytes. No TLS certificate validation is disabled.
Upstream check-disabling cloud hotfixes, fabricated launcher manifests, and
unimplemented profile-token verification are not enabled. Profile-token requests
fail explicitly rather than receiving fabricated verification success.

The local content-pages adapter removes the provider's `LawinServer V2` lobby
notice from both emergency-notice formats. Other notices and version-specific
backgrounds are preserved. Upstream license and credits remain in this guide
and in the deployment.

An original `DefaultGame.ini` cloud file replaces the frontend playlist entries
with BR Solo as the enabled default. Content pages likewise expose only
`Playlist_DefaultSolo`; neither change implements a game server. Cloud hashes
and lengths describe the exact served bytes. The real 12.50 lobby renders SOLO.
The client's observed ALL/OPEN party preferences are now accepted as local
presentation metadata, while authentication, single-account membership and
namespace isolation remain enforced. There are still no public discovery,
remote membership or invitation routes. Historical failures below describe their
recorded revisions, not the latest behavior.

An actual Play attempt originally reached an unmatched ticket route (404).
An original authenticated failure route now explicitly reports HTTP503,
`fol.matchmaking.game_server_unavailable`, because no verified game server is
running. It refuses to invent a ticket, signature or readiness. This is not a
matchmaking implementation. Profile-token verification still
returns explicit 501. [Game-server work](../../docs/GAMEPLAY_SERVERS.md) is separate
from this backend and remains unverified.

`lawin.log` contains route-template/status/process events, not request bodies,
raw URLs, credentials, tokens or key data. `lawin-service.log` and `mongodb.log`
are owner-only raw diagnostic logs; inspect/redact before sharing. The original
game’s prefix log may contain the disposable local password.

## Verification

`node --test integrations/lawin/*.test.cjs` uses synthetic fixtures only.
CTest runs it when Node is available; CI provisions Node22. The separate
`smoke.cjs` requires a running isolated provider and checks wrong-password
rejection, real password login, token verification, common_core and keychain
transport, lobby-notice removal and the Season 12 background. It also verifies
Athena starter references, outfit switching,
unknown-outfit rejection and restoration of the original selection. It never
prints passwords, tokens or keychain contents.
Pass the private Lawin configuration as its second argument to also test a real
WebSocket SASL login, resource binding and XMPP session.

Actual sessions83effcb2 ande4ce3df3 pass QueryKeychain and reach the game's
`Completed` login stage and HandleLoginRequestComplete Result2. This does **not**
prove a stable logged-in menu: the startup-error dialog persists, profile-token
verification returns501, and the game requests exit about50 seconds after launch.
All owned processes were cleaned up; no EAC was observed during sampled monitoring.

Session `2026-09-29_23-17-45_bc59bd91` additionally verifies the **real game**
accepts the original cloud configuration and logs XMPP LoggedIn. The backend
records authenticated/bound/session-ready/presence. Party creation now reaches
the HTTP CreateParty request, which remains unreachable because the local URL
selector does not yet include party routes. This is the next integration task.
The startup error remains: LogAppES closing code1 precedes the profile-token501
response. Do not assume that fabricating a successful verification would fix it.

Session `2026-09-29_23-22-29_63f1dcea` verifies the next step: **Successfully
created party**, persistent party state **Active**, followed by accepted party
and member metadata updates. The original party service now maintains one private
local member, enforces access-token authentication/account ownership, revisions,
bounded metadata and leave semantics. The URL selector includes only Fortnite's
party API, not arbitrary service paths. Unsupported operations/config changes
return explicit failures; in particular the client's request to change party
visibility is not silently acknowledged. No public listing, invitations,
matchmaking, remote members or multiplayer support is claimed.
The same independent startup dialog and profile-token failure still prevent a
verified functional lobby. Party creation is not gameplay or lobby completion.

The profile model disables Mongoose empty-object minimization. Existing local
profiles whose `items` field was omitted are repaired with an empty object using
an account-scoped, absence-conditional update. Existing inventories are never
replaced; malformed inventories fail startup for investigation. This corrects
the real client's `Field items was not found` / null-object errors. The HTTP smoke
now checks the actual full-profile response field, not only its HTTP status.

Session `2026-09-29_23-32-24_269d0947` confirms those profile errors are absent,
while the startup rejection remains. Proton Hotfix session
`2026-09-29_23-29-32_86cfbc45`, using its separate owned prefix, reproduced the
same active-party/startup-error outcome as Experimental. Both imported runtimes
report bundled Wine11.0; system Wine11.18 is a separate installation.
The profile-token diagnostic records bounded schema names/types/lengths, never
scalar values. Current requests contain a nested object with three short strings,
none matching a local access token. This does not establish a verification
contract or justify returning unverified success.
