// SPDX-License-Identifier: GPL-3.0-only
'use strict';

// The pinned provider advertises itself through both generations of the lobby
// emergency-notice schema. Keep other notices; expose only the requested BR Solo.
function register(app, getContentPages) {
  app.get('/content/api/pages/*', (req, res) => {
    const pages = getContentPages(req);
    const playlists = pages.playlistinformation?.playlist_info;
    if (Array.isArray(playlists?.playlists))
      playlists.playlists = playlists.playlists.filter(entry =>
        entry.playlist_name === 'Playlist_DefaultSolo');
    for (const [section, field] of [
      [pages.emergencynotice?.news, 'messages'],
      [pages.emergencynoticev2?.emergencynotices, 'emergencynotices']
    ]) {
      if (Array.isArray(section?.[field]))
        section[field] = section[field].filter(notice => notice.title !== 'LawinServer V2');
    }
    res.json(pages);
  });
}
module.exports = {register};
