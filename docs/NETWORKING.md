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

## Service-address configuration experiment

`--local-backend-probe --backend-probe-method service-ini` separately tests only
Protocol=http and Domain=127.0.0.1:3551 under OnlineSubsystemMcp.BaseServiceMcp.
It does not add epicapp=Localhost, edit game files, disable certificate checking
or provide credentials. It retains the context/isolation/12.50 gates. The default
method remains `selector`, preserving the prior experiment for reproducibility.

Evidence for this hypothesis is the matching section name in the supplied binary,
the author's [UEFN configuration example](https://gist.github.com/AntogamerYT/c540ddb72f9e819c626d7487c8b9fcdf),
and Epic's [configuration-override documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/configuration-files-in-unreal-engine).
UEFN/newer-engine examples are not proof of support in 12.50's 4.25 shipping build;
the address must be observed in actual runtime behavior before any listener or
login workflow is considered usable. No unrelated example settings are adopted.

Actual command-line probe `2026-09-29_19-13-02_9401e23d` recorded the requested
INI argument but retained the production backend host; no loopback URL appeared
outside the command line. A separate prefix-file probe
`2026-09-29_19-14-23_242fb8aa` placed only the two address keys in the exact
WindowsClient/Engine.ini location identified by the game's plugin log. That also
retained the production host. Both remained isolated, logged manual sign-in and
kept bVerifyPeer=true. Neither establishes private routing or login. The temporary
prefix file was moved into the second session as tested-Engine.ini afterward;
no pre-existing Engine.ini was overwritten. The experiment's source template is
profiles/experiments/service-address.ini, not an enabled default.

Future saved server records and browser adapters should support hostnames, IPv4,
IPv6 and user-managed tunnels. They must remain optional for local development.

## Verified request observation, not routing

An original opt-in Windows diagnostic module now observes real12.50 HTTP method
calls inside the isolated owned child. Session2026-09-29_20-33-09_cdc67e77 validated
seven request objects and restored its method pointer. Categories include automatic
token-endpoint calls; no request URL, authentication payload, header or body is
logged or changed. There is still no local listener or working redirect. The next
integration must keep the game and private service in the same isolated namespace,
enable only that namespace's loopback, and authenticate explicit local development
identities. It must not reuse production credentials or depend on Epic services.
