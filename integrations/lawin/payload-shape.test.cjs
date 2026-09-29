// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const {describe} = require('./payload-shape.cjs');
test('payload schema never emits scalar contents or arbitrary keys', () => {
  const value = {payload: {token: 'synthetic-secret-token', 'secret-property': 123456,
    accountId: 'synthetic-private-identifier', code: 812345, valid: true, data: null}};
  const result = describe(value, token => token === 'synthetic-secret-token');
  const serialized = JSON.stringify(result);
  for (const secret of ['synthetic-secret-token','secret-property','synthetic-private-identifier','812345','123456'])
    assert.ok(!serialized.includes(secret));
  assert.equal(result.children[0].children[0].matches_local_access_token, true);
  assert.equal(result.children[0].children[5].type, 'null');
});
test('wide, nested and cyclic inputs have bounded output', () => {
  const cycle = {}; cycle.data = cycle;
  assert.ok(JSON.stringify(describe(cycle)).includes('"truncated":true'));
  const result = describe(Array.from({length: 1000}, () => ({data: Array(100).fill('secret')})));
  let count = 0;
  const walk = node => { count++; (node.children || []).forEach(walk); };
  walk(result);
  assert.ok(count <= 48);
  assert.equal(result.truncated, true);
  assert.ok(!JSON.stringify(result).includes('secret'));
});
