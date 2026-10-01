// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const net = require('node:net');
const http = require('node:http');
const {once} = require('node:events');
const {relayConfig, forward, healthy} = require('./relay.cjs');

test('remote config needs an IPv4 server and credentials', () => {
  const good = {server: '192.168.5.52', username: 'nocto@tux', password: 'xsp944kdfe'};
  assert.equal(relayConfig(good).server, '192.168.5.52');
  for (const bad of [{...good, server: 'example.com'}, {...good, server: '300.1.1.1'}, {...good, server: '127.0.0.1'},
    {...good, username: ''}, {...good, password: 'a\nb'}, {...good, password: 'x'.repeat(129)}, undefined])
    assert.throws(() => relayConfig(bad));
});

test('connections to the loopback port reach the server and come back', async () => {
  const upstream = net.createServer(socket => socket.on('data', data => socket.end('echo:' + data)));
  upstream.listen(0, '127.0.0.1');
  await once(upstream, 'listening');
  const events = [];
  const relay = await forward(3551, '127.0.0.1', event => events.push(event),
    {listenPort: 0, targetPort: upstream.address().port});
  const client = net.connect(relay.address().port, '127.0.0.1');
  client.end('hello');
  let reply = '';
  client.on('data', chunk => reply += chunk);
  await once(client, 'close');
  assert.equal(reply, 'echo:hello');
  relay.close(); upstream.close();
});

test('an unreachable server closes the connection and is recorded', async () => {
  const unused = net.createServer().listen(0, '127.0.0.1');
  await once(unused, 'listening');
  const port = unused.address().port;
  unused.close();
  const events = [];
  const relay = await forward(3551, '127.0.0.1', event => events.push(event), {listenPort: 0, targetPort: port});
  const client = net.connect(relay.address().port, '127.0.0.1');
  client.on('error', () => {});
  await once(client, 'close');
  assert.deepEqual(events, [{event: 'relay-connect-failed', port: 3551}]);
  relay.close();
});

test('health check sends the loopback Host the server expects', async () => {
  let host;
  const server = http.createServer((req, res) => {
    host = req.headers.host;
    res.end(JSON.stringify({backend: 'lawin-v2', ready: host === '127.0.0.1:3551'}));
  }).listen(0, '127.0.0.1');
  await once(server, 'listening');
  const port = server.address().port;
  assert.equal(await healthy('127.0.0.1', port), true);
  assert.equal(host, '127.0.0.1:3551');
  server.close();
  await once(server, 'close');
  assert.equal(await healthy('127.0.0.1', port), false);
});
