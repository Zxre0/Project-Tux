// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const {missingInventories, repairEmptyInventories} = require('./profile-schema.cjs');
test('inventory repair identifies absence without replacing existing data', () => {
  const profiles = {common_core: {}, common_public: {items: {}}, athena: {items: {existing: {}}},
    'untrusted.path': {}};
  assert.deepEqual(missingInventories(profiles), ['common_core']);
  assert.equal(Object.hasOwn(profiles.common_core, 'items'), false);
  for (const value of [null, [], 'invalid'])
    assert.throws(() => missingInventories({common_core: {items: value}}), /not overwritten/);
});
test('repair uses fixed paths, account scope and absence precondition', async () => {
  const calls = [];
  const Model = {schema: {set: (key, value) => calls.push({key, value})},
    findOne: query => { assert.deepEqual(query, {accountId: 'local-account'});
      return {lean: async () => ({profiles: {common_core: {}}})}; },
    updateOne: async (filter, update) => { calls.push({filter, update}); return {modifiedCount: 1}; }};
  assert.equal(await repairEmptyInventories(Model, 'local-account'), 1);
  assert.deepEqual(calls, [{key: 'minimize', value: false},
    {filter: {accountId: 'local-account', 'profiles.common_core.items': {$exists: false}},
      update: {$set: {'profiles.common_core.items': {}}}}]);
});
