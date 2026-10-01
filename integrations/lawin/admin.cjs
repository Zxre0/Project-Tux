// SPDX-License-Identifier: GPL-3.0-only
// Offline account/locker/shop editor for the local Lawin deployment, used by
// the Project Tux launcher. Runs only while no game session owns the database:
// starts a private mongod, applies one command, prints JSON and stops it.
//
//   node admin.cjs LAWIN_CONFIG info
//   node admin.cjs LAWIN_CONFIG set-name NAME
//   node admin.cjs LAWIN_CONFIG set-stats '{"level":100,"book_level":100,"book_purchased":true,"vbucks":5000}'
//   node admin.cjs LAWIN_CONFIG grant TEMPLATE_ID...
//   node admin.cjs LAWIN_CONFIG remove TEMPLATE_ID...
//   node admin.cjs LAWIN_CONFIG grant-all | reset-locker
//   node admin.cjs LAWIN_CONFIG cosmetics          # every template ID Lawin knows
//   node admin.cjs LAWIN_CONFIG shop | shop-set FILE
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const {spawn} = require('node:child_process');
const {createRequire} = require('node:module');
const {deployment} = require('./config.cjs');
const locker = require('./local-locker.cjs');

const TEMPLATE = /^Athena[A-Za-z]+:[A-Za-z0-9_\-.]{1,96}$/;
const NAME = /^[A-Za-z0-9 _.\-]{3,24}$/;
const object = value => value && typeof value === 'object' && !Array.isArray(value);

// ---- pure profile edits (unit tested) ----
function bump(profile) {
  profile.rvn++;
  profile.commandRevision++;
  profile.updated = new Date().toISOString();
  return profile;
}
function newItem(templateId) {
  return {templateId, attributes: {item_seen: true, variants: [], favorite: false}, quantity: 1};
}
function grant(athena, templateIds) {
  const next = structuredClone(athena);
  let added = 0;
  for (const id of templateIds) {
    if (!TEMPLATE.test(id) || id.startsWith('AthenaCosmeticLocker')) throw new Error(`Invalid cosmetic ID: ${id}`);
    if (!Object.values(next.items).some(item => item.templateId.toLowerCase() === id.toLowerCase())) {
      next.items[id] = newItem(id);
      ++added;
    }
  }
  return {profile: added ? bump(next) : next, added};
}
function remove(athena, templateIds) {
  const next = structuredClone(athena);
  const equipped = new Set(Object.values(next.items)
    .filter(item => item.templateId === 'CosmeticLocker:cosmeticlocker_athena')
    .flatMap(item => Object.values(item.attributes?.locker_slots_data?.slots || {}))
    .flatMap(slot => slot.items || []).map(id => String(id).toLowerCase()));
  let removed = 0;
  for (const id of templateIds) {
    if (equipped.has(id.toLowerCase())) throw new Error(`Unequip ${id} before removing it`);
    for (const [key, item] of Object.entries(next.items))
      if (item.templateId.toLowerCase() === id.toLowerCase()) { delete next.items[key]; ++removed; }
  }
  return {profile: removed ? bump(next) : next, removed};
}
function resetLocker(athena) {
  const empty = structuredClone(athena);
  empty.items = {};
  Object.assign(empty.stats.attributes, {loadouts: [locker.lockerId], active_loadout_index: 0,
    last_applied_loadout: locker.lockerId});
  return locker.plan(empty);
}
function setStats(athena, common, stats) {
  const nextAthena = structuredClone(athena), nextCommon = structuredClone(common);
  const attributes = nextAthena.stats.attributes;
  const integer = (value, max) => Number.isSafeInteger(value) && value >= 0 && value <= max;
  let athenaChanged = false, commonChanged = false;
  for (const key of ['level', 'book_level', 'accountLevel']) {
    if (stats[key] === undefined) continue;
    if (!integer(stats[key], 1000)) throw new Error(`${key} must be 0-1000`);
    attributes[key] = stats[key];
    athenaChanged = true;
  }
  if (stats.book_purchased !== undefined) {
    if (typeof stats.book_purchased !== 'boolean') throw new Error('book_purchased must be true/false');
    attributes.book_purchased = stats.book_purchased;
    athenaChanged = true;
  }
  if (stats.vbucks !== undefined) {
    if (!integer(stats.vbucks, 1e9)) throw new Error('vbucks must be 0-1000000000');
    for (const [key, item] of Object.entries(nextCommon.items))
      if (item.templateId.toLowerCase().startsWith('currency:mtx')) delete nextCommon.items[key];
    nextCommon.items['Currency:MtxPurchased'] = {templateId: 'Currency:MtxPurchased',
      attributes: {platform: 'Shared'}, quantity: stats.vbucks};
    commonChanged = true;
  }
  return {athena: athenaChanged ? bump(nextAthena) : nextAthena,
    common: commonChanged ? bump(nextCommon) : nextCommon};
}
function summary(user, athena, common) {
  const attributes = athena.stats.attributes;
  const vbucks = Object.values(common.items)
    .filter(item => item.templateId.toLowerCase().startsWith('currency:mtx'))
    .reduce((sum, item) => sum + (item.quantity || 0), 0);
  const counts = {};
  for (const item of Object.values(athena.items)) {
    const type = item.templateId.split(':')[0];
    if (type !== 'CosmeticLocker') counts[type] = (counts[type] || 0) + 1;
  }
  return {username: user.username, accountId: user.accountId, level: attributes.level ?? 1,
    book_level: attributes.book_level ?? 1, book_purchased: !!attributes.book_purchased,
    accountLevel: attributes.accountLevel ?? 1, vbucks, cosmetics: counts,
    owned: Object.values(athena.items).map(item => item.templateId)
      .filter(id => !id.startsWith('CosmeticLocker')).sort()};
}
function validateShop(shop) {
  if (!object(shop)) throw new Error('Shop must be a JSON object');
  for (const [key, entry] of Object.entries(shop)) {
    if (key === '//') continue;
    if (!/^(daily|featured)\d{1,2}$/.test(key) || !object(entry) || !Array.isArray(entry.itemGrants) ||
        !entry.itemGrants.length || !entry.itemGrants.every(id => TEMPLATE.test(id)) ||
        !Number.isSafeInteger(entry.price) || entry.price < 0)
      throw new Error(`Invalid shop entry ${key}: needs itemGrants [IDs] and a price`);
  }
  return shop;
}

// ---- database session ----
async function withDatabase(config, login, action) {
  const lock = path.join(config.root, 'session.lock');
  try { fs.mkdirSync(lock); }
  catch { throw new Error('The backend is in use by a running game session; stop it first'); }
  const root = path.join(config.root, 'source');
  const upstream = createRequire(path.join(root, 'index.js'));
  const mongoose = upstream('mongoose');
  const mongod = spawn(config.mongod, ['--dbpath', path.join(config.root, 'database'), '--bind_ip',
    '127.0.0.1', '--port', '27017', '--nounixsocket', '--wiredTigerCacheSizeGB', '0.25'],
    {stdio: 'ignore'});
  try {
    await mongoose.connect('mongodb://127.0.0.1:27017/fol_lawin', {serverSelectionTimeoutMS: 15000});
    const User = upstream('./model/user.js');
    const Profile = upstream('./model/profiles.js');
    Profile.schema.set('minimize', false);
    const user = await User.findOne({email: login.username});
    if (!user) throw new Error('No local account yet; launch the game once first');
    const document = await Profile.findOne({accountId: user.accountId}).lean();
    if (!document?.profiles?.athena || !document.profiles.common_core)
      throw new Error('Local profile is incomplete');
    return await action({User, Profile, user, document, root});
  } finally {
    await mongoose.disconnect().catch(() => {});
    mongod.kill('SIGTERM');
    await new Promise(resolve => mongod.exitCode !== null ? resolve() : mongod.once('exit', resolve));
    fs.rmdirSync(lock);
  }
}

async function main() {
  process.umask(0o077);
  const [configFile, command, ...args] = process.argv.slice(2);
  const {config, login} = deployment(configFile);
  const source = path.join(config.root, 'source');
  const shopFile = path.join(source, 'Config/catalog_config.json');
  const allCosmetics = () => Object.values(JSON.parse(fs.readFileSync(
    path.join(source, 'Config/DefaultProfiles/athena.json'), 'utf8')).items)
    .map(item => item.templateId).filter(id => !id.startsWith('CosmeticLocker')).sort();

  if (command === 'cosmetics') return {cosmetics: allCosmetics()};
  if (command === 'shop') return {shop: JSON.parse(fs.readFileSync(shopFile, 'utf8'))};
  if (command === 'shop-set') {
    const shop = validateShop(JSON.parse(fs.readFileSync(args[0], 'utf8')));
    fs.copyFileSync(shopFile, shopFile + '.bak');
    fs.writeFileSync(shopFile, JSON.stringify(shop, null, 4));
    return {shop};
  }
  return withDatabase(config, login, async ({User, Profile, user, document}) => {
    let athena = document.profiles.athena, common = document.profiles.common_core;
    const save = async (fields, result) => {
      const update = {};
      if (fields.athena) update['profiles.athena'] = fields.athena;
      if (fields.common) update['profiles.common_core'] = fields.common;
      if (Object.keys(update).length) {
        const backups = path.join(config.root, 'profile-backups');
        fs.mkdirSync(backups, {recursive: true, mode: 0o700});
        fs.writeFileSync(path.join(backups, `admin-${command}-${Date.now()}.json`),
          JSON.stringify({accountId: user.accountId, athena, common_core: common}), {mode: 0o600});
        await Profile.updateOne({accountId: user.accountId}, {$set: update});
      }
      return {...result, ...summary(user, fields.athena || athena, fields.common || common)};
    };
    switch (command) {
    case 'info':
      return summary(user, athena, common);
    case 'set-name': {
      const name = args.join(' ').trim();
      if (!NAME.test(name)) throw new Error('Name must be 3-24 letters, digits, spaces, _ . -');
      if (await User.exists({username_lower: name.toLowerCase(), accountId: {$ne: user.accountId}}))
        throw new Error('That name is taken');
      await User.updateOne({accountId: user.accountId}, {$set: {username: name, username_lower: name.toLowerCase()}});
      user.username = name;
      return summary(user, athena, common);
    }
    case 'set-stats': {
      const next = setStats(athena, common, JSON.parse(args[0] || '{}'));
      return save(next, {});
    }
    case 'grant': {
      const {profile, added} = grant(athena, args);
      return save({athena: profile}, {added});
    }
    case 'grant-all': {
      const {profile, added} = grant(athena, allCosmetics());
      return save({athena: profile}, {added});
    }
    case 'remove': {
      const {profile, removed} = remove(athena, args);
      return save({athena: profile}, {removed});
    }
    case 'reset-locker':
      return save({athena: resetLocker(athena)}, {reset: true});
    default:
      throw new Error(`Unknown command: ${command}`);
    }
  });
}

if (require.main === module)
  // Synchronous write: process.exit() would truncate a large reply on a pipe.
  main().then(result => { fs.writeSync(1, JSON.stringify({ok: true, ...result}) + '\n'); process.exit(0); },
    error => { fs.writeSync(1, JSON.stringify({ok: false, error: String(error.message || error)}) + '\n');
      process.exit(1); });
module.exports = {grant, remove, resetLocker, setStats, summary, validateShop};
