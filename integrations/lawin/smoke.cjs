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
  console.log(JSON.stringify({test: 'lawin-http', invalid_password: 'rejected',
    password: 'accepted', verify: 'accepted', profile: 'accepted', keychain_entries: data.length}));
}
main().catch(err => { console.error(err.message); process.exit(1); });
