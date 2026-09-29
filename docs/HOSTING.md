# Hosting

`fortnite-linux host 12.61` currently returns an explicit integration blocker.
No game server has been started. A module being PE x64 with matching manifest
hash does not establish that loading it is authorized, safe, or compatible.
No module injection or authentication replacement is implemented.

The future host manager is separate from client launch and backend adapters.
Default bind is loopback; UDP 7777 is the proposed game port. Playlist, players,
bots and console commands must be advertised by a validated module/version
capability set. Backend availability and UDP binding are different readiness
conditions. UPnP defaults off and is not implemented. No public advertisement or
automatic router/firewall changes occur.

The current `network ADDRESS PORT` command tests a local UDP bind and releases it;
it does not reserve the port for a later host and does not establish reachability.
