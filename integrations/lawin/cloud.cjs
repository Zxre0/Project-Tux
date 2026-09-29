// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const crypto = require('node:crypto');
// Only local transport settings. No check-disabling upstream hotfixes.
const engine = Buffer.from('[OnlineSubsystemMcp.Xmpp]\nServerAddr="ws://127.0.0.1:8080"\nServerPort=8080\n' +
  '[OnlineSubsystemMcp.Xmpp Prod]\nServerAddr="ws://127.0.0.1:8080"\nServerPort=8080\n' +
  '[XMPP]\nbEnableWebsockets=true\n');
function register(app) {
  const filename = 'DefaultEngine.ini';
  const metadata = {uniqueFilename: filename, filename,
    hash: crypto.createHash('sha1').update(engine).digest('hex'),
    hash256: crypto.createHash('sha256').update(engine).digest('hex'),
    length: engine.length, contentType: 'application/octet-stream',
    uploaded: '2026-09-29T00:00:00.000Z', storageType: 'S3', storageIds: {}, doNotCache: true};
  app.get('/fortnite/api/cloudstorage/system', (req, res) => res.json([metadata]));
  app.get('/fortnite/api/cloudstorage/system/DefaultEngine.ini', (req, res) =>
    res.type('application/octet-stream').send(engine));
  app.all('/fortnite/api/cloudstorage/system/*', (req, res) => res.sendStatus(404));
}
module.exports = {register, engine};
