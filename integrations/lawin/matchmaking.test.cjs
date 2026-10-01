// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {register, udpPortBound, gameServer} = require('./matchmaking.cjs');

function fixture(authenticate = async (id, token) => token === 'fixture-token', ready = () => true) {
  const routes = {};
  const events = [];
  const app = {get: (p, h) => routes['GET ' + p] = h, post: (p, h) => routes['POST ' + p] = h};
  register(app, {accountId: 'local-account', authenticate, record: v => events.push(v),
    serverReady: ready});
  return {events, routes, async request(route, {token, params = {}, query = {}} = {}) {
    let status = 200, body;
    const response = {sendStatus: v => status = v,
      status(v) { status = v; return this; }, json: v => body = v};
    await routes[route]({headers: {authorization: token}, params, query}, response);
    return {status, body};
  }};
}
const TICKET = 'GET /fortnite/api/game/v2/matchmakingservice/ticket/player/:accountId';
const SESSION = 'GET /fortnite/api/matchmaking/session/:sessionId';

test('tickets require verified local authorization and account ownership', async () => {
  const f = fixture();
  for (const token of [undefined, 'bearer wrong', 'Bearer fixture-token', ['bearer fixture-token']])
    assert.equal((await f.request(TICKET, {token, params: {accountId: 'local-account'}})).status, 401);
  assert.equal((await f.request(TICKET, {token: 'bearer fixture-token',
    params: {accountId: 'another-account'}})).status, 403);
  const broken = fixture(async () => { throw new Error('private diagnostic'); });
  assert.equal((await broken.request(TICKET, {token: 'bearer fixture-token',
    params: {accountId: 'local-account'}})).status, 500);
  assert.ok(!JSON.stringify(broken.events).includes('private diagnostic'));
});

test('ticket points at the local matchmaker and session at the local game server', async () => {
  const f = fixture();
  const auth = {token: 'bearer fixture-token', params: {accountId: 'local-account'}};
  assert.equal((await f.request(TICKET, {...auth, query: {bucketId: 'bad'}})).status, 400);
  const ticket = await f.request(TICKET, {...auth,
    query: {bucketId: '12895384:0:NAE:playlist_defaultsolo'}});
  assert.equal(ticket.status, 200);
  assert.match(ticket.body.serviceUrl, /^ws:\/\/127\.0\.0\.1:\d+$/);
  const session = await f.request(SESSION, {token: 'bearer fixture-token',
    params: {sessionId: 'abc'}});
  assert.equal(session.body.serverAddress, '127.0.0.1');
  assert.equal(session.body.serverPort, 7777);
  assert.equal(session.body.buildUniqueId, '12895384');
  assert.ok(!JSON.stringify(f.events).includes('fixture-token'));
});

test('udp readiness parses /proc/net/udp format', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'fol-udp-'));
  const file = path.join(dir, 'udp');
  const header = '  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode ref pointer drops\n';
  fs.writeFileSync(file, header + '   0: 00000000:1E61 00000000:0000 07 00000000:00000000 00:00000000 00000000  1000        0 1 2 0 0\n');
  assert.equal(udpPortBound(7777, [file]), true);
  fs.writeFileSync(file, header);
  assert.equal(udpPortBound(7777, [file]), false);
  fs.rmSync(dir, {recursive: true});
});

test('FOL_GAME_SERVER selects a remote server, falling back to loopback', () => {
  assert.deepEqual(gameServer(''), {address: '127.0.0.1', port: 7777, remote: false});
  assert.deepEqual(gameServer('192.168.4.20'), {address: '192.168.4.20', port: 7777, remote: true});
  assert.deepEqual(gameServer('100.101.102.103:7778'), {address: '100.101.102.103', port: 7778, remote: true});
  assert.equal(gameServer('999.1.1.1').remote, false);
  assert.equal(gameServer('example.com').remote, false);
});
