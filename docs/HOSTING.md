# Hosting

`fortnite-linux host 12.50` currently returns an explicit integration blocker.
No game server has been started. A module being PE x64 with matching manifest
hash does not establish that loading it is authorized, safe, or compatible.
No gameplay-module loading or game-server admission integration is implemented.
The separate client diagnostic bootstrap loads only its scoped observation/local
router helpers, not a hosting module.

The active scope is ordinary BR Solo on 12.50. Alternative server references
Erbium and Nova have been inspected; Erbium's Windows research DLL builds on this
Linux host but has not been loaded. Nova is not a backend replacement and has no
verified 12.50 runtime test. See [gameplay evidence](GAMEPLAY_SERVERS.md) and
[Nova analysis](NOVA_ANALYSIS.md). Normal hosting remains blocked; no fabricated
matchmaking ticket is issued for a nonexistent server.

The future host manager is separate from client launch and backend adapters.
Default bind is loopback; UDP 7777 is the proposed game port. Playlist, players,
bots and console commands must be advertised by a validated module/version
capability set. Backend availability and UDP binding are different readiness
conditions. UPnP defaults off and is not implemented. No public advertisement or
automatic router/firewall changes occur.

The current `network ADDRESS PORT` command tests a local UDP bind and releases it;
it does not reserve the port for a later host and does not establish reachability.
