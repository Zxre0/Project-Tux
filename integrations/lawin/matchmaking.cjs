// SPDX-License-Identifier: GPL-3.0-only
'use strict';

// An authenticated failure contract, not a matchmaking or readiness emulator.
// Do not issue a ticket until a reviewed compatible host actually exists.
function register(app, {accountId, authenticate, record}) {
  app.get('/fortnite/api/game/v2/matchmakingservice/ticket/player/:accountId', async (req, res) => {
    const bearer = req.headers.authorization;
    try {
      if (typeof bearer !== 'string' || !bearer.startsWith('bearer ') ||
          !await authenticate(accountId, bearer.slice(7))) return res.sendStatus(401);
      if (req.params.accountId !== accountId) return res.sendStatus(403);
      record({event: 'matchmaking-unavailable', component: 'game-server',
        reason: 'integration-unimplemented'});
      return res.status(503).json({errorCode: 'fol.matchmaking.game_server_unavailable',
        errorMessage: 'No verified private Fortnite 12.50 BR Solo game server is running.',
        component: 'game-server', readiness: 'unavailable'});
    } catch {
      record({event: 'matchmaking-request-failed', component: 'backend'});
      return res.sendStatus(500);
    }
  });
}
module.exports = {register};
