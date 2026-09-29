# Networking

`network 127.0.0.1 7777` probes an exclusive local UDP bind. IPv6 loopback `::1`
and addresses assigned to local interfaces are accepted. Invalid/nonlocal bind
addresses and out-of-range ports are rejected. Firewall state is explicitly
untested; binding does not imply LAN or WAN reachability. No UPnP mapping exists.

Offline launch probes use unprivileged user/network namespaces and cannot contact
the host's loopback server. A future private-network integration requires a
verified game/backend interface before introducing server-address arguments.
No arbitrary backend redirect or production authentication route is assumed.

Future saved server records and browser adapters should support hostnames, IPv4,
IPv6 and user-managed tunnels. They must remain optional for local development.
