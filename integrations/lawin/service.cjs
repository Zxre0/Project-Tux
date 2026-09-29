// SPDX-License-Identifier: GPL-3.0-only
// Original local entry point for pinned LawinServerV2; no upstream index.js/Discord startup.
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { createRequire } = require('node:module');
const {deployment, requireIsolation} = require('./config.cjs');
let stage = 'load-deployment';

async function main() {
  process.umask(0o077);
  requireIsolation();
  const {config, login} = deployment(process.argv[2]);
  const root = path.join(config.root, 'source');
  process.chdir(root);
  const upstream = createRequire(path.join(root, 'index.js'));
  const express = upstream('express');
  const mongoose = upstream('mongoose');
  const bcrypt = upstream('bcrypt');
  stage = 'connect-database';
  await mongoose.connect('mongodb://127.0.0.1:27017/fol_lawin', {serverSelectionTimeoutMS: 10000});
  stage = 'prepare-local-account';
  const User = upstream('./model/user.js');
  const Profile = upstream('./model/profiles.js');
  const Friends = upstream('./model/friends.js');
  let user = await User.findOne({email: login.username});
  if (!user) {
    const accountId = crypto.randomUUID().replaceAll('-', '');
    const profiles = upstream('./structs/profile.js').createProfiles(accountId);
    // Local empty inventory, not a copied full locker or real account entitlements.
    for (const profile of Object.values(profiles)) profile.items = {};
    user = await User.create({created: new Date(), discordId: 'fol-local-player', accountId,
      username: 'Local Player', username_lower: 'local player', email: login.username,
      password: await bcrypt.hash(login.password, 10)});
    await Profile.create({created: new Date(), accountId, profiles});
    await Friends.create({created: new Date(), accountId});
  } else if (!await bcrypt.compare(login.password, user.password)) {
    throw new Error('Existing local account password differs; no automatic overwrite');
  }
  if (!await Profile.exists({accountId: user.accountId}) ||
      !await Friends.exists({accountId: user.accountId})) throw new Error('Incomplete local account');
  global.JWT_SECRET = crypto.randomBytes(32).toString('hex');
  global.accessTokens = [];
  global.refreshTokens = [];
  global.clientTokens = [];
  global.exchangeCodes = [];
  global.Clients = [];
  global.MUCs = {};
  stage = 'load-routes';
  const app = express();
  app.use(express.json({limit: '32kb'}));
  app.use(express.urlencoded({extended: false, limit: '32kb'}));
  app.use((req, res, next) => {
    res.on('finish', () => {
      // Route templates are source constants; no raw URL, body, tokens or account IDs.
      const grant = ['password','client_credentials','refresh_token'].includes(req.body?.grant_type)
        ? req.body.grant_type : undefined;
      process.send?.({event: 'http', method: req.method, route: req.route?.path || 'unmatched',
        status: res.statusCode, grant});
    });
    if (!['127.0.0.1:3551','localhost:3551'].includes(req.headers.host)) return res.sendStatus(403);
    const bearer = req.headers.authorization;
    if (bearer && /^bearer /i.test(bearer)) req.headers.authorization = 'bearer ' + bearer.slice(7);
    next();
  });
  app.get('/fol/health', (req, res) => res.json({backend: 'lawin-v2', ready: true}));
  // Do not adopt upstream platform-check disabling hotfixes or fabricated manifests.
  require('./cloud.cjs').register(app);
  app.use('/fortnite/api/game/v2/profileToken', (req, res) => {
    const fields = ['token', 'profileToken', 'profile_token', 'accountId', 'signature'];
    const body = req.body && typeof req.body === 'object' ? req.body : {};
    const metadata = Object.entries(body).slice(0, 16).map(([key, value]) => ({
      field: fields.includes(key) ? key : 'unrecognized', type: typeof value,
      length: typeof value === 'string' ? value.length : undefined,
      matches_local_access_token: typeof value === 'string' &&
        global.accessTokens.some(entry => entry.token === value)
    }));
    process.send?.({event: 'profile-token-unimplemented', fields: metadata,
      content_type: ['application/json','application/x-www-form-urlencoded'].includes(
        (req.headers['content-type'] || '').split(';')[0]) ? req.headers['content-type'].split(';')[0] : 'other',
      authenticated_local_session: global.accessTokens.some(entry =>
        req.headers.authorization === 'bearer ' + entry.token)});
    res.sendStatus(501);
  });
  app.use(['/Builds', '/launcher/api/public/assets'],
    (req, res) => res.sendStatus(501));
  for (const route of ['auth','user','lightswitch','version','cloudstorage','storefront',
    'contentpages','friends','timeline','mcp','main']) app.use(upstream(`./routes/${route}.js`));
  app.use((req, res) => res.sendStatus(404));
  app.use((err, req, res, next) => {
    process.send?.({event: 'route-error', code: 'upstream-handler-failed'});
    res.sendStatus(500);
  });
  stage = 'start-local-xmpp';
  await require('./xmpp.cjs').listen({upstream, record: data => process.send?.(data),
    authenticate: async (accountId, token) => {
      if (accountId !== user.accountId || !global.accessTokens.some(entry =>
        entry.accountId === accountId && entry.token === token) || !token.startsWith('eg1~')) return null;
      try {
        const claims = upstream('jsonwebtoken').verify(token.slice(4), global.JWT_SECRET,
          {algorithms: ['HS256']});
        return claims.sub === accountId && await User.exists({accountId, banned: false}) ? accountId : null;
      } catch { return null; } // Authentication rejection, never accepted on verifier failure.
    }});
  app.listen(3551, '127.0.0.1', () => process.send?.({event: 'ready'}));
}
main().catch(err => {
  process.send?.({event: 'service-failed', stage,
    code: /^[A-Z_0-9]{1,40}$/.test(String(err.code)) ? err.code : 'SERVICE_ERROR'});
  process.exit(1);
});
