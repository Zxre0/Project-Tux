// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const crypto = require('node:crypto');
// Only local transport and Solo playlist presentation. No check-disabling hotfixes.
const engine = Buffer.from('[OnlineSubsystemMcp.Xmpp]\nServerAddr="ws://127.0.0.1:8080"\nServerPort=8080\n' +
  '[OnlineSubsystemMcp.Xmpp Prod]\nServerAddr="ws://127.0.0.1:8080"\nServerPort=8080\n' +
  '[XMPP]\nbEnableWebsockets=true\n');
const game = Buffer.from('[/Script/FortniteGame.FortGameInstance]\n' +
  '!FrontEndPlaylistData=ClearArray\n' +
  '+FrontEndPlaylistData=(PlaylistName=Playlist_DefaultSolo, PlaylistAccess=' +
  '(bEnabled=True,bIsDefaultPlaylist=True,bVisibleWhenDisabled=False,bDisplayAsNew=False,' +
  'CategoryIndex=0,bDisplayAsLimitedTime=False,DisplayPriority=3))\n');
function register(app) {
  const files = [['DefaultEngine.ini', engine], ['DefaultGame.ini', game]];
  const metadata = files.map(([filename, data]) => ({uniqueFilename: filename, filename,
    hash: crypto.createHash('sha1').update(data).digest('hex'),
    hash256: crypto.createHash('sha256').update(data).digest('hex'),
    length: data.length, contentType: 'application/octet-stream',
    uploaded: '2026-09-30T00:00:00.000Z', storageType: 'S3', storageIds: {}, doNotCache: true}));
  app.get('/fortnite/api/cloudstorage/system', (req, res) => res.json(metadata));
  for (const [filename, data] of files)
    app.get('/fortnite/api/cloudstorage/system/' + filename, (req, res) =>
      res.type('application/octet-stream').send(data));
  app.all('/fortnite/api/cloudstorage/system/*', (req, res) => res.sendStatus(404));
}
module.exports = {register, engine, game};
