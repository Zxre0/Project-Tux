// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const {grant, remove, resetLocker, setStats, summary, validateShop} = require('./admin.cjs');
const {plan, lockerId} = require('./local-locker.cjs');

function starter() {
  return plan({rvn: 1, commandRevision: 1, items: {}, stats: {attributes: {loadouts: [lockerId],
    active_loadout_index: 0, last_applied_loadout: lockerId}}});
}
const common = () => ({rvn: 1, commandRevision: 1, items: {}, stats: {attributes: {}}});

test('grant adds new cosmetics once and bumps the revision', () => {
  const athena = starter();
  const {profile, added} = grant(athena, ['AthenaCharacter:cid_028_athena_commando_f',
    'AthenaPickaxe:defaultpickaxe']);
  assert.equal(added, 1);
  assert.equal(profile.rvn, athena.rvn + 1);
  assert.ok(profile.items['AthenaCharacter:cid_028_athena_commando_f']);
  assert.throws(() => grant(athena, ['bad id']), /Invalid cosmetic ID/);
});

test('remove refuses equipped items', () => {
  const athena = grant(starter(), ['AthenaGlider:glider_id_001']).profile;
  assert.throws(() => remove(athena, ['AthenaPickaxe:defaultpickaxe']), /Unequip/);
  assert.equal(remove(athena, ['AthenaGlider:glider_id_001']).removed, 1);
});

test('reset-locker restores the starter locker', () => {
  const athena = grant(starter(), ['AthenaGlider:glider_id_001']).profile;
  const reset = resetLocker(athena);
  assert.equal(reset.items['AthenaGlider:glider_id_001'], undefined);
  assert.ok(reset.items[lockerId]);
});

test('set-stats validates and writes level, battle pass and V-Bucks', () => {
  const {athena, common: core} = setStats(starter(), common(),
    {level: 50, book_level: 70, book_purchased: true, vbucks: 13500});
  const info = summary({username: 'Tux', accountId: 'a'}, athena, core);
  assert.equal(info.level, 50);
  assert.equal(info.book_level, 70);
  assert.equal(info.book_purchased, true);
  assert.equal(info.vbucks, 13500);
  assert.throws(() => setStats(starter(), common(), {level: -1}), /level/);
  assert.throws(() => setStats(starter(), common(), {vbucks: 'lots'}), /vbucks/);
});

test('shop validation', () => {
  assert.ok(validateShop({daily1: {itemGrants: ['AthenaCharacter:cid_001_athena_commando_f_default'], price: 800}}));
  assert.throws(() => validateShop({daily1: {itemGrants: [], price: 1}}), /Invalid shop entry/);
  assert.throws(() => validateShop({weird: {itemGrants: ['AthenaCharacter:x'], price: 1}}), /Invalid shop entry/);
});
