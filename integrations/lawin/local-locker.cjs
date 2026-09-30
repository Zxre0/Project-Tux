// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
// Original local-development metadata only. All eight original default outfits
// predate the supplied 12.50 build; assets remain in the user's installation.
const outfits = Array.from({length: 8}, (_, i) =>
  `AthenaCharacter:cid_${String(i + 1).padStart(3, '0')}_athena_commando_${i < 4 ? 'f' : 'm'}_default`);
const defaults = {Character: outfits[4], Pickaxe: 'AthenaPickaxe:defaultpickaxe',
  Glider: 'AthenaGlider:defaultglider', Dance: 'AthenaDance:eid_dancemoves'};
const lockerId = 'lawin-loadout'; // Preserve the dangling ID used by the pinned template.
const object = value => value && typeof value === 'object' && !Array.isArray(value);

function plan(profile) {
  if (!object(profile) || !object(profile.items) || !object(profile.stats?.attributes))
    throw new Error('Malformed local Athena profile; not overwritten');
  const attributes = profile.stats.attributes;
  if (Object.keys(profile.items).length) {
    const active = profile.items[attributes.loadouts?.[attributes.active_loadout_index]];
    if (active?.templateId !== 'CosmeticLocker:cosmeticlocker_athena')
      throw new Error('Existing nonempty inventory has no valid active locker; manual repair needed');
    return null; // Never replace a user's existing inventory or equipped outfit.
  }
  if (JSON.stringify(attributes.loadouts) !== JSON.stringify([lockerId]) ||
      attributes.active_loadout_index !== 0 || attributes.last_applied_loadout !== lockerId ||
      !Number.isSafeInteger(profile.rvn) || !Number.isSafeInteger(profile.commandRevision))
    throw new Error('Unrecognized empty locker state; not overwritten');
  const next = structuredClone(profile);
  const slots = {};
  for (const category of ['Character','Backpack','Pickaxe','Glider','SkyDiveContrail',
    'MusicPack','LoadingScreen','Dance','ItemWrap']) {
    const count = category === 'Dance' ? 6 : category === 'ItemWrap' ? 7 : 1;
    const items = Array(count).fill('');
    if (defaults[category]) items[0] = defaults[category];
    slots[category] = {items};
    if (!['Dance','MusicPack','LoadingScreen'].includes(category))
      slots[category].activeVariants = Array(count).fill(null);
  }
  for (const templateId of [...outfits, defaults.Pickaxe, defaults.Glider, defaults.Dance])
    next.items[templateId] = {templateId, attributes: {item_seen: true, variants: []}, quantity: 1};
  next.items[lockerId] = {templateId: 'CosmeticLocker:cosmeticlocker_athena', quantity: 1,
    attributes: {locker_name: 'Linux Starter', item_seen: true, use_count: 0,
      banner_icon_template: attributes.banner_icon || '',
      banner_color_template: attributes.banner_color || '', locker_slots_data: {slots}}};
  for (const [category, templateId] of Object.entries(defaults))
    next.stats.attributes[`favorite_${category.toLowerCase()}`] =
      category === 'Dance' ? slots.Dance.items : templateId;
  next.rvn++;
  next.commandRevision++;
  next.updated = new Date().toISOString();
  return next;
}

async function prepare(Profile, accountId, backupDirectory) {
  const document = await Profile.findOne({accountId}).lean();
  const before = document?.profiles?.athena;
  const next = plan(before);
  if (!next) return {status: 'existing-preserved'};
  fs.mkdirSync(backupDirectory, {recursive: true, mode: 0o700});
  const backup = path.join(backupDirectory, `athena-before-starter-${crypto.randomUUID()}.json`);
  fs.writeFileSync(backup, JSON.stringify({accountId, profile: before}, null, 2),
    {flag: 'wx', mode: 0o600});
  // The service prepares profiles before opening HTTP; the session also owns the
  // database lock. Compare the whole snapshot to reject even an unexpected writer.
  const result = await Profile.updateOne({accountId, 'profiles.athena': before},
    {$set: {'profiles.athena': next}});
  if (result.modifiedCount !== 1) throw new Error('Athena profile changed during starter setup');
  return {status: 'starter-created', outfits: outfits.length, backup: path.basename(backup)};
}
module.exports = {outfits, defaults, lockerId, plan, prepare};
