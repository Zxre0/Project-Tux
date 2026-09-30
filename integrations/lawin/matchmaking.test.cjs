// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const {register} = require('./matchmaking.cjs');

function fixture(authenticate = async (id, token) => token === 'fixture-token') {
  let handler;
  const events = [];
  register({get: (path, callback) => {
    assert.equal(path, '/fortnite/api/game/v2/matchmakingservice/ticket/player/:accountId');
    handler = callback;
  }}, {accountId: 'local-account', authenticate, record: value => events.push(value)});
  return {events, async request(token, accountId = 'local-account') {
    let status, body;
    const response = {sendStatus: value => status = value,
      status(value) { status = value; return this; }, json: value => body = value};
    await handler({headers: {authorization: token}, params: {accountId}}, response);
    return {status, body};
  }};
}

test('tickets require verified local authorization and account ownership', async () => {
  const f = fixture();
  for (const token of [undefined, 'bearer wrong', 'Bearer fixture-token', ['bearer fixture-token']])
    assert.equal((await f.request(token)).status, 401);
  assert.equal((await f.request('bearer fixture-token', 'another-account')).status, 403);
  assert.deepEqual(f.events, []);
});

test('authenticated Play reports unavailable without manufacturing a ticket or readiness', async () => {
  const f = fixture();
  const result = await f.request('bearer fixture-token');
  assert.equal(result.status, 503);
  assert.equal(result.body.errorCode, 'fol.matchmaking.game_server_unavailable');
  assert.equal(result.body.component, 'game-server');
  assert.equal(result.body.readiness, 'unavailable');
  for (const key of ['ticket', 'payload', 'signature', 'serviceUrl', 'sessionId'])
    assert.equal(Object.hasOwn(result.body, key), false);
  assert.equal(f.events[0].reason, 'integration-unimplemented');
  assert.ok(!JSON.stringify(f.events).includes('fixture-token'));
  const broken = fixture(async () => { throw new Error('private diagnostic'); });
  assert.equal((await broken.request('bearer fixture-token')).status, 500);
  assert.ok(!JSON.stringify(broken.events).includes('private diagnostic'));
});
