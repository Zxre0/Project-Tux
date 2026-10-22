// SPDX-License-Identifier: GPL-3.0-only
'use strict';

// The pinned provider advertises itself through both generations of the lobby
// emergency-notice schema, the news feeds and (shown as a pop-up by older builds
// such as 3.1) the login message. Keep other notices; expose only BR Solo.
const promotion = new Set(['LawinServer V2', 'Join my discord server!']);
function register(app, getContentPages) {
  app.get('/content/api/pages/*', (req, res) => {
    const pages = getContentPages(req);
    const playlists = pages.playlistinformation?.playlist_info;
    if (Array.isArray(playlists?.playlists))
      playlists.playlists = playlists.playlists.filter(entry =>
        entry.playlist_name === 'Playlist_DefaultSolo');
    for (const [section, field] of [
      [pages.emergencynotice?.news, 'messages'],
      [pages.emergencynoticev2?.emergencynotices, 'emergencynotices'],
      [pages.battleroyalenews?.news, 'messages'],
      [pages.savetheworldnews?.news, 'messages'],
      [pages.creativenews?.news, 'messages']
    ]) {
      if (Array.isArray(section?.[field]))
        section[field] = section[field].filter(notice => !promotion.has(notice.title));
    }
    if (promotion.has(pages.loginmessage?.loginmessage?.message?.title))
      delete pages.loginmessage;
    res.json(pages);
  });
}
module.exports = {register};
