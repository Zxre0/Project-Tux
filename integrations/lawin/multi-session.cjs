// SPDX-License-Identifier: GPL-3.0-only
// The local game server and the client sign in with the same local account.
// Upstream LawinServerV2 revokes an account's existing tokens on every new
// login, which logs the server out ("Logged In Elsewhere") and tears down the
// match. Keep earlier sessions valid; JWT expiry is still enforced on verify.
'use strict';
function preserveSessions(lists = () => [global.accessTokens, global.refreshTokens]) {
  return (req, res, next) => {
    const saved = lists().map(list => [...list]);
    res.on('finish', () => lists().forEach((list, i) => {
      for (const entry of saved[i]) if (!list.includes(entry)) list.push(entry);
    }));
    next();
  };
}
module.exports = {preserveSessions};
