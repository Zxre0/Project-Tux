# Networking

`network 127.0.0.1 7777` probes an exclusive local UDP bind. IPv6 loopback `::1`
and addresses assigned to local interfaces are accepted. Invalid/nonlocal bind
addresses and out-of-range ports are rejected. Firewall state is explicitly
untested; binding does not imply LAN or WAN reachability. No UPnP mapping exists.

On bind failure, `socket_creation_probe` separately tests native UDP socket
creation in the requested address family and records its errno/error. This
distinguishes sandbox-denied creation from a port collision without interpreting
Qt's sometimes-generic `Unknown error`. A successful creation probe does not
prove bind availability or remote reachability. No traffic is sent by this probe.
An earlier restricted sandbox denied IPv4/IPv6 socket creation with errno 1
(`Operation not permitted`); that was not a router/firewall diagnosis. Access has
since been restored. On 2026-09-29 the current host successfully probed the local
127.0.0.1:7777 UDP bind and released it. This does not establish firewall, remote
reachability or availability inside a separate game namespace. The native
implementation remains in `platform_linux.cpp`.

Offline launch probes use unprivileged user/network namespaces and cannot contact
the host's loopback server. A future private-network integration requires a
verified game/backend interface before introducing server-address arguments.
No arbitrary backend redirect or production authentication route is assumed.

12.50 built-in-option probe `2026-09-29_18-46-46_f124f659` tested
MCPCONFIG=localhost inside the existing isolated launch. The game recorded the
argument but still selected its normal production backend with an empty override;
requests remained blocked by isolation. Thus `--local-backend-probe` is a negative
discovery experiment, not a working local redirect or host-server connection.
It requires both --offline-probe and --launcher-context-probe and is not a default.
The current profile now tests the separate `-epicapp=Localhost` selector based on
the binary's explicit epicapp/backend diagnostics; the earlier MCPCONFIG result
must not be attributed to this new command. No HTTP backend, XMPP service or game
server was started for the MCPCONFIG test.

Epicapp follow-up `2026-09-29_18-53-34_b7a20b71` recognizes Localhost but logs a
missing Fortnite MCPConfig for service fortnite and host `<unconfigured>`, then
rejects the backend. A listener alone cannot fix an absent game-side service
configuration. No working HTTP route, authentication or private join is implied.

Future saved server records and browser adapters should support hostnames, IPv4,
IPv6 and user-managed tunnels. They must remain optional for local development.
