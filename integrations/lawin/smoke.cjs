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
  const headers = {authorization: 'bearer ' + session.access_token};
  const verified = await fetch(base + '/account/api/oauth/verify', {headers});
  if (verified.status !== 200) throw new Error('verify-failed');
  const profile = await fetch(base + '/fortnite/api/game/v2/profile/' + session.account_id +
    '/client/QueryProfile?profileId=common_core&rvn=-1', {method: 'POST',
      headers: {...headers, 'content-type': 'application/json'}, body: '{}'});
  if (profile.status !== 200) throw new Error('profile-failed');
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
