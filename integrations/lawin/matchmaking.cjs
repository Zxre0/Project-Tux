// SPDX-License-Identifier: GPL-3.0-only
// Local BR matchmaking for one private game server on 127.0.0.1:7777.
// Protocol reference: LawinServerV2 2205696 routes/matchmaking.js and
// matchmaker/matchmaker.js. The matchmaker only sends "Play" once the game
// server's UDP port is actually bound in this network namespace.
'use strict';
const fs = require('node:fs');
const crypto = require('node:crypto');

// FOL_GAME_SERVER=ip:port points matchmaking at another player's server
// (multiplayer "join" mode); otherwise the local server on loopback.
function gameServer(value = process.env.FOL_GAME_SERVER) {
  const match = /^(\d{1,3}(?:\.\d{1,3}){3})(?::(\d{1,5}))?$/.exec(value || '');
  if (!match || match[1].split('.').some(octet => Number(octet) > 255))
    return {address: '127.0.0.1', port: 7777, remote: false};
  const port = Number(match[2] || 7777);
  return {address: match[1], port: port >= 1 && port <= 65535 ? port : 7777,
    remote: match[1] !== '127.0.0.1'};
}
const GAME_SERVER = gameServer();
const MATCHMAKER_PORT = 3552;
const id = () => crypto.randomUUID().replaceAll('-', '');

function udpPortBound(port, files = ['/proc/net/udp', '/proc/net/udp6']) {
  const hex = ':' + port.toString(16).toUpperCase().padStart(4, '0');
  for (const file of files) {
    let text;
    try { text = fs.readFileSync(file, 'utf8'); } catch { continue; }
    for (const line of text.split('\n').slice(1)) {
      const local = line.trim().split(/\s+/)[1];
      if (local?.endsWith(hex)) return true;
    }
  }
  return false;
}

// A remote server's readiness can't be observed from here; send Play at once.
const localReady = () => GAME_SERVER.remote || udpPortBound(GAME_SERVER.port);
function register(app, {accountId, authenticate, record, serverReady = localReady}) {
  const buckets = new Map();
  const authorized = handler => async (req, res) => {
    const bearer = req.headers.authorization;
    try {
      if (typeof bearer !== 'string' || !bearer.startsWith('bearer ') ||
          !await authenticate(accountId, bearer.slice(7))) return res.sendStatus(401);
      if (req.params.accountId && req.params.accountId !== accountId) return res.sendStatus(403);
      return handler(req, res);
    } catch {
      record({event: 'matchmaking-request-failed', component: 'backend'});
      return res.sendStatus(500);
    }
  };
  app.get('/fortnite/api/game/v2/matchmakingservice/ticket/player/:accountId', authorized((req, res) => {
    const bucket = typeof req.query?.bucketId === 'string' ? req.query.bucketId.split(':') : [];
    if (bucket.length !== 4) return res.sendStatus(400);
    buckets.set(accountId, bucket[0]);
    record({event: 'matchmaking-ticket', playlist: /^[A-Za-z0-9_]{1,64}$/.test(bucket[3])
      ? bucket[3] : 'other', game_server_ready: serverReady()});
    return res.json({serviceUrl: `ws://127.0.0.1:${MATCHMAKER_PORT}`, ticketType: 'mms-player',
      payload: Buffer.from(bucket.join(':')).toString('base64'), signature: 'fol-local'});
  }));
  app.get('/fortnite/api/game/v2/matchmaking/account/:accountId/session/:sessionId',
    authorized((req, res) => res.json({accountId, sessionId: req.params.sessionId, key: 'none'})));
  app.get('/fortnite/api/matchmaking/session/findPlayer/:accountId', (req, res) => res.sendStatus(200));
  app.post('/fortnite/api/matchmaking/session/matchMakingRequest', (req, res) => res.json([]));
  app.post('/fortnite/api/matchmaking/session/:sessionId/join', (req, res) => res.sendStatus(204));
  app.get('/fortnite/api/matchmaking/session/:sessionId', authorized((req, res) => {
    record({event: 'matchmaking-session', game_server_ready: serverReady()});
    return res.json({
      id: req.params.sessionId, ownerId: id().toUpperCase(), ownerName: '[DS]fol-local',
      serverName: '[DS]fol-local', serverAddress: GAME_SERVER.address, serverPort: GAME_SERVER.port,
      maxPublicPlayers: 100, openPublicPlayers: 99, maxPrivatePlayers: 0, openPrivatePlayers: 0,
      attributes: {REGION_s: 'NAE', GAMEMODE_s: 'FORTATHENA', ALLOWBROADCASTING_b: true,
        SUBREGION_s: 'VA', DCID_s: 'FOL-LOCAL', tenant_s: 'Fortnite', MATCHMAKINGPOOL_s: 'Any',
        STORMSHIELDDEFENSETYPE_i: 0, HOTFIXVERSION_i: 0, PLAYLISTNAME_s: 'Playlist_DefaultSolo',
        SESSIONKEY_s: id().toUpperCase(), TENANT_s: 'Fortnite', BEACONPORT_i: 15009},
      publicPlayers: [], privatePlayers: [], totalPlayers: 1, allowJoinInProgress: false,
      shouldAdvertise: false, isDedicated: false, usesStats: false, allowInvites: false,
      usesPresence: false, allowJoinViaPresence: true, allowJoinViaPresenceFriendsOnly: false,
      buildUniqueId: buckets.get(accountId) || '0', lastUpdated: new Date().toISOString(),
      started: false});
  }));
}

// Matchmaker websocket: Connecting -> Waiting -> Queued (until the server is
// bound) -> SessionAssignment -> Play.
async function listen({upstream, record, serverReady = localReady,
  port = MATCHMAKER_PORT, pollMs = 1000}) {
  const {WebSocketServer} = upstream('ws');
  const server = new WebSocketServer({host: '127.0.0.1', port, maxPayload: 32768,
    perMessageDeflate: false});
  server.on('connection', async socket => {
    const send = (name, payload) => socket.readyState === 1 &&
      socket.send(JSON.stringify({payload, name}));
    const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
    const ticketId = id(), matchId = id(), sessionId = id();
    record({event: 'matchmaker-connected'});
    send('StatusUpdate', {state: 'Connecting'});
    await wait(500);
    send('StatusUpdate', {totalPlayers: 1, connectedPlayers: 1, state: 'Waiting'});
    await wait(500);
    send('StatusUpdate', {ticketId, queuedPlayers: 0, estimatedWaitSec: 0, status: {},
      state: 'Queued'});
    while (socket.readyState === 1 && !serverReady()) await wait(pollMs);
    if (socket.readyState !== 1) return record({event: 'matchmaker-abandoned'});
    send('StatusUpdate', {matchId, state: 'SessionAssignment'});
    await wait(1000);
    send('Play', {matchId, sessionId, joinDelaySec: 1});
    record({event: 'matchmaker-play-sent'});
  });
  await new Promise((resolve, reject) => { server.once('listening', resolve); server.once('error', reject); });
  server.on('error', () => record({event: 'matchmaker-server-error'}));
  record({event: 'matchmaker-listening', address: '127.0.0.1', port});
  return server;
}
module.exports = {register, listen, udpPortBound, gameServer, GAME_SERVER, MATCHMAKER_PORT};
