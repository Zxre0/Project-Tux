// SPDX-License-Identifier: GPL-3.0-only
// Real HTTP contract checks; never print credentials, tokens or keychain contents.
'use strict';
const fs = require('node:fs');
async function main() {
  const login = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
  const base = 'http://127.0.0.1:3551';
  const basic = 'Basic ' + Buffer.from('fol-local-app:fol-local-secret').toString('base64');
  async function grant(password) {
    return fetch(base + '/account/api/oauth/token', {method: 'POST',
      headers: {authorization: basic, 'content-type': 'application/x-www-form-urlencoded'},
      body: new URLSearchParams({grant_type: 'password', username: login.username, password})});
  }
  const wrong = await grant('wrong-local-password');
  if (wrong.status !== 400) throw new Error('bad-password-not-rejected');
  const authenticated = await grant(login.password);
  if (authenticated.status !== 200) throw new Error('password-grant-failed');
  const session = await authenticated.json();
  const headers = {authorization: 'bearer ' + session.access_token,
    'user-agent': 'Fortnite/++Fortnite+Release-12.50-CL-13137020 Windows/10.0.0.0.64bit'};
  const verified = await fetch(base + '/account/api/oauth/verify', {headers});
  if (verified.status !== 200) throw new Error('verify-failed');
  const profile = await fetch(base + '/fortnite/api/game/v2/profile/' + session.account_id +
    '/client/QueryProfile?profileId=common_core&rvn=-1', {method: 'POST',
      headers: {...headers, 'content-type': 'application/json'}, body: '{}'});
  if (profile.status !== 200) throw new Error('profile-failed');
  const snapshot = (await profile.json()).profileChanges?.find(change =>
    change.changeType === 'fullProfileUpdate')?.profile;
  if (!snapshot || !Object.hasOwn(snapshot, 'items') || !snapshot.items ||
      typeof snapshot.items !== 'object' || Array.isArray(snapshot.items))
    throw new Error('profile-inventory-field-missing');
  const {outfits, defaults} = require('./local-locker.cjs');
  async function athenaCommand(operation, body = {}) {
    return fetch(base + '/fortnite/api/game/v2/profile/' + session.account_id +
      '/client/' + operation + '?profileId=athena&rvn=-1', {method: 'POST',
        headers: {...headers, 'content-type': 'application/json'}, body: JSON.stringify(body)});
  }
  async function athena() {
    const response = await athenaCommand('QueryProfile');
    if (response.status !== 200) throw new Error('athena-query-failed');
    const data = await response.json();
    return data.profileChanges?.find(x => x.changeType === 'fullProfileUpdate')?.profile;
  }
  const beforeLocker = await athena();
  const lockerId = beforeLocker?.stats.attributes.loadouts[beforeLocker.stats.attributes.active_loadout_index];
  const slots = beforeLocker?.items[lockerId]?.attributes.locker_slots_data.slots;
  if (!slots || !slots.Character.items[0] || !slots.Pickaxe.items[0] || !slots.Glider.items[0] ||
      !outfits.every(id => beforeLocker.items[id]?.templateId === id) ||
      !beforeLocker.items[defaults.Pickaxe] || !beforeLocker.items[defaults.Glider])
    throw new Error('starter-locker-contract-failed');
  const previousOutfit = slots.Character.items[0];
  const alternate = outfits.find(id => id !== previousOutfit);
  const equip = itemToSlot => athenaCommand('SetCosmeticLockerSlot',
    {lockerItem: lockerId, category: 'Character', itemToSlot, slotIndex: 0, variantUpdates: []});
  try {
    if ((await equip(alternate)).status !== 200) throw new Error('outfit-equip-failed');
    const equipped = await athena();
    if (equipped.items[lockerId].attributes.locker_slots_data.slots.Character.items[0] !== alternate ||
        equipped.rvn <= beforeLocker.rvn) throw new Error('outfit-equip-not-persisted');
    if ((await equip('AthenaCharacter:fol_nonexistent')).status !== 400)
      throw new Error('unknown-outfit-not-rejected');
  } finally {
    if ((await equip(previousOutfit)).status !== 200) throw new Error('original-outfit-restore-failed');
  }
  if ((await athena()).items[lockerId].attributes.locker_slots_data.slots.Character.items[0] !== previousOutfit)
    throw new Error('original-outfit-not-persisted');
  console.log(JSON.stringify({test: 'lawin-local-locker', outfits: outfits.length,
    equip: 'persisted', unknown_outfit: 'rejected', original_outfit: 'restored'}));
  const keys = await fetch(base + '/fortnite/api/storefront/v2/keychain', {headers});
  const data = await keys.json();
  if (keys.status !== 200 || !Array.isArray(data) || !data.every(x => typeof x === 'string'))
    throw new Error('keychain-contract-failed');
  const partyBase = base + '/party/api/v1/Fortnite';
  if ((await fetch(partyBase + '/user/' + session.account_id)).status !== 401)
    throw new Error('party-no-auth-not-rejected');
  if ((await fetch(partyBase + '/user/' + '0'.repeat(32), {headers})).status !== 403)
    throw new Error('party-other-account-not-rejected');
  const created = await fetch(partyBase + '/parties', {method: 'POST',
    headers: {...headers, 'content-type': 'application/json'}, body: JSON.stringify({
      config: {max_size: 16}, join_info: {connection: {
        id: session.account_id + '@prod.ol.epicgames.com/fol-smoke'}}, meta: {test: 'local'}})});
  if (created.status !== 200) throw new Error('party-create-failed');
  const party = await created.json();
  const summary = await (await fetch(partyBase + '/user/' + session.account_id, {headers})).json();
  if (summary.current?.[0]?.id !== party.id) throw new Error('party-not-persisted');
  if ((await fetch(partyBase + '/parties/' + party.id + '/members/' + session.account_id,
    {method: 'DELETE', headers})).status !== 204) throw new Error('party-leave-failed');
  console.log(JSON.stringify({test: 'lawin-party', authorization: 'enforced',
    create: 'accepted', query: 'persisted', leave: 'accepted'}));
  if (process.argv[3]) {
    const {config} = require('./config.cjs').deployment(process.argv[3]);
    const {createRequire} = require('node:module');
    const WebSocket = createRequire(config.root + '/source/index.js')('ws');
    const socket = new WebSocket('ws://127.0.0.1:8080', 'xmpp');
    await new Promise((resolve, reject) => {
      const timer = setTimeout(() => { socket.terminate(); reject(new Error('xmpp-timeout')); }, 5000);
      let authenticated = false, bound = false;
      socket.on('error', reject);
      const open = '<open xmlns="urn:ietf:params:xml:ns:xmpp-framing" to="prod.ol.epicgames.com" version="1.0"/>';
      socket.on('open', () => socket.send(open));
      socket.on('message', buffer => {
        const message = buffer.toString();
        if (message.includes('<stream:features') && !authenticated)
          socket.send('<auth xmlns="urn:ietf:params:xml:ns:xmpp-sasl" mechanism="PLAIN">' +
            Buffer.from(`\0${session.account_id}\0${session.access_token}`).toString('base64') + '</auth>');
        else if (message.includes('<success')) { authenticated = true; socket.send(open); }
        else if (message.includes('<stream:features') && authenticated)
          socket.send('<iq type="set" id="bind"><bind xmlns="urn:ietf:params:xml:ns:xmpp-bind"><resource>fol-smoke</resource></bind></iq>');
        else if (message.includes('id="bind"') && message.includes('<jid>')) {
          bound = true;
          socket.send('<iq type="set" id="session"><session xmlns="urn:ietf:params:xml:ns:xmpp-session"/></iq>');
        } else if (bound && message.includes('id="session"') && message.includes('type="result"')) {
          clearTimeout(timer); socket.close(); resolve();
        }
      });
    });
    console.log(JSON.stringify({test: 'lawin-xmpp', authenticated: true, bound: true, session: true}));
  }
  console.log(JSON.stringify({test: 'lawin-http', invalid_password: 'rejected',
    password: 'accepted', verify: 'accepted', profile: 'accepted', keychain_entries: data.length}));
}
main().catch(err => { console.error(err.message); process.exit(1); });
