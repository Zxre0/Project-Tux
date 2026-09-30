// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const {store} = require('./party.cjs');
const account = 'a'.repeat(32);
const input = () => ({config: {max_size: 16}, join_info: {connection: {
  id: account + '@prod.ol.epicgames.com/V2:Fortnite:WIN', meta: {platform: 'WIN'}},
  meta: {'player:ready': 'false'}}, meta: {'party:test': 'true'}});
test('single-member party creation, persistence, revisions and leaving', () => {
  const s = store(account); assert.deepEqual(s.summary().current, []);
  const party = s.create(input()); assert.equal(party.members[0].account_id, account);
  assert.equal(party.config.discoverability, 'INVITED_ONLY');
  assert.deepEqual(s.summary().current[0], party);
  party.meta['party:test'] = 'mutated'; assert.equal(s.get(party.id).meta['party:test'], 'true');
  assert.throws(() => s.create(input()), /already_exists/);
  assert.throws(() => s.patch(party.id, {revision: 1}), /revision_conflict/);
  assert.throws(() => s.get('missing'), /not_found/);
  s.patch(party.id, {revision: 0, meta: {update: {new: 'value'}, delete: ['party:test']}});
  assert.deepEqual(s.get(party.id).meta, {new: 'value'});
  assert.equal(s.get(party.id).revision, 1);
  s.patch(party.id, {revision: 0, meta: {update: {ready: 'true'}}}, true);
  assert.equal(s.get(party.id).members[0].revision, 1);
  assert.throws(() => s.patch(party.id, {revision: 1, config: {discoverability: 'WORLD'}}),
    /unsupported_config/);
  assert.equal(s.get(party.id).revision, 1);
  s.patch(party.id, {revision: 1, config: {discoverability: 'ALL', joinability: 'OPEN'}});
  assert.equal(s.get(party.id).config.discoverability, 'ALL');
  assert.equal(s.get(party.id).config.joinability, 'OPEN');
  assert.equal(s.get(party.id).members.length, 1);
  s.patch(party.id, {revision: 2, config: {max_size: 8}});
  assert.equal(s.get(party.id).config.max_size, 8);
  s.remove(party.id); assert.deepEqual(s.summary().current, []);
});
test('forged membership and malformed metadata/config do not create party state', () => {
  for (const change of [x => x.join_info.connection.id = 'someone@else',
    x => x.config.max_size = 999, x => x.meta = {nested: {}},
    x => x.meta = JSON.parse('{"__proto__":"bad"}')]) {
    const s = store(account), body = input(); change(body);
    assert.throws(() => s.create(body)); assert.deepEqual(s.summary().current, []);
  }
});
