// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const {session} = require('./xmpp.cjs');
const {engine, register} = require('./cloud.cjs');
const account = 'a'.repeat(32);
const open = {name: 'open', attributes: {xmlns: 'urn:ietf:params:xml:ns:xmpp-framing'}};
const auth = (user = account, token = 'local-test-token') => ({name: 'auth',
  attributes: {xmlns: 'urn:ietf:params:xml:ns:xmpp-sasl', mechanism: 'PLAIN'},
  content: Buffer.from(`\0${user}\0${token}`).toString('base64')});
function fixture() {
  const sent = [], events = []; let closed = false;
  const peer = session({send: x => sent.push(x), close: () => closed = true,
    authenticate: async (id, token) => id === account && token === 'local-test-token' ? id : null,
    record: x => events.push(x)});
  return {peer, sent, events, closed: () => closed};
}
test('XMPP enforces local identity, token, mechanism and handshake order', async () => {
  for (const request of [auth(account, 'wrong'), auth('b'.repeat(32)),
    {...auth(), attributes: {...auth().attributes, mechanism: 'ANONYMOUS'}},
    {name: 'iq', attributes: {id: 'early', type: 'set'}}]) {
    const f = fixture(); await f.peer.handle(open); await f.peer.handle(request);
    assert.equal(f.closed(), true); assert.ok(!f.sent.some(x => x.includes('<success')));
    assert.ok(!JSON.stringify(f.events).includes('local-test-token'));
  }
  const f = fixture(); await f.peer.handle(auth()); assert.equal(f.closed(), true);
});
test('authenticated stream binds and handles session and ping without inventing features', async () => {
  const f = fixture();
  await f.peer.handle(open); await f.peer.handle(auth()); await f.peer.handle(open);
  assert.ok(f.sent.some(x => x.includes('<success')));
  await f.peer.handle({name: 'iq', attributes: {id: 'bind', type: 'set'}, children: [{name: 'bind',
    attributes: {xmlns: 'urn:ietf:params:xml:ns:xmpp-bind'}, children: [{name: 'resource', content: 'V2:Fortnite:WIN'}]}]});
  await f.peer.handle({name: 'iq', attributes: {id: 'session', type: 'set'}, children: [{name: 'session',
    attributes: {xmlns: 'urn:ietf:params:xml:ns:xmpp-session'}}]});
  await f.peer.handle({name: 'iq', attributes: {id: 'ping', type: 'get'}, children: [{name: 'ping',
    attributes: {xmlns: 'urn:xmpp:ping'}}]});
  assert.equal(f.closed(), false);
  assert.ok(f.events.some(e => e.event === 'xmpp-session-ready'));
  assert.ok(f.sent.at(-1).includes('type="result" id="ping"'));
  await f.peer.handle({name: 'iq', attributes: {id: 'unsupported', type: 'get'}, children: []});
  assert.ok(f.sent.at(-1).includes('service-unavailable'));
  await f.peer.handle({name: 'presence', attributes: {to: 'party-missing@muc.example'}});
  assert.equal(f.closed(), true); // No invented party room membership.
});
test('cloud index hashes describe exactly the original loopback-only configuration', () => {
  const routes = new Map(); register({get: (p, f) => routes.set(p, f), all: () => {}});
  let metadata;
  routes.get('/fortnite/api/cloudstorage/system')({}, {json: value => metadata = value[0]});
  const crypto = require('node:crypto');
  assert.equal(metadata.hash256, crypto.createHash('sha256').update(engine).digest('hex'));
  assert.equal(metadata.length, engine.length);
  assert.equal(metadata.filename, 'DefaultEngine.ini');
  const settings = engine.toString().split('\n').filter(x => x && !x.startsWith('['));
  assert.ok(settings.every(x => ['ServerAddr="ws://127.0.0.1:8080"', 'ServerPort=8080',
    'bEnableWebsockets=true'].includes(x)));
});
