// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {plan, prepare, outfits, defaults, lockerId} = require('./local-locker.cjs');
const empty = () => ({rvn: 4, commandRevision: 3, items: {}, stats: {attributes: {
  loadouts: [lockerId], active_loadout_index: 0, last_applied_loadout: lockerId, xp: 123}}});
test('starter repairs references and supplies eight original outfits without assets', () => {
  const before = empty(), after = plan(before);
  assert.deepEqual(before.items, {});
  assert.equal(Object.keys(after.items).length, 12);
  assert.equal(after.stats.attributes.xp, 123);
  assert.equal(after.rvn, 5);
  assert.equal(after.commandRevision, 4);
  const slots = after.items[lockerId].attributes.locker_slots_data.slots;
  for (const [category, id] of Object.entries(defaults)) {
    assert.equal(slots[category].items[0], id);
    assert.equal(after.items[id].templateId, id);
  }
  assert.equal(new Set(outfits).size, 8);
  assert.equal(slots.Dance.items.length, 6);
  assert.equal(slots.ItemWrap.items.length, 7);
});
test('existing outfits and preferences survive repeated initialization', () => {
  const equipped = plan(empty());
  equipped.stats.attributes.favorite_character = outfits[1];
  equipped.items[lockerId].attributes.locker_slots_data.slots.Character.items = [outfits[1]];
  const copy = structuredClone(equipped);
  assert.equal(plan(equipped), null);
  assert.deepEqual(equipped, copy);
  const unrelated = empty(); unrelated.items.custom = {};
  assert.throws(() => plan(unrelated), /nonempty inventory/);
  const unknown = empty(); unknown.stats.attributes.loadouts = ['user-locker'];
  assert.throws(() => plan(unknown), /Unrecognized/);
});
test('migration is account scoped, backed up and rejects concurrent mutation', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'fol-locker-test-'));
  try {
    const before = empty();
    const Profile = {findOne: query => {
      assert.deepEqual(query, {accountId: 'local'});
      return {lean: async () => ({profiles: {athena: before}})};
    }, updateOne: async (filter, update) => {
      assert.deepEqual(filter, {accountId: 'local', 'profiles.athena': before});
      assert.equal(update.$set['profiles.athena'].items[defaults.Character].quantity, 1);
      const files = fs.readdirSync(directory);
      assert.equal(files.length, 1);
      const backup = path.join(directory, files[0]);
      assert.equal(fs.statSync(backup).mode & 0o777, 0o600);
      assert.deepEqual(JSON.parse(fs.readFileSync(backup)), {accountId: 'local', profile: before});
      return {modifiedCount: 0};
    }};
    await assert.rejects(prepare(Profile, 'local', directory), /changed during/);
  } finally { fs.rmSync(directory, {recursive: true}); }
});
