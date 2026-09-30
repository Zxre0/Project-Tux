// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const {register} = require('./contentpages.cjs');

test('local content exposes only BR Solo while preserving background and unrelated notices', () => {
  let handler;
  register({get: (path, callback) => {
    assert.equal(path, '/content/api/pages/*'); handler = callback;
  }}, () => ({
    playlistinformation: {playlist_info: {playlists: [
      {playlist_name: 'Playlist_DefaultSolo', image: 'existing-solo-image'},
      {playlist_name: 'Playlist_DefaultDuo'}, {playlist_name: 'Playlist_PlaygroundV2'}]}},
    dynamicbackgrounds: {backgrounds: {backgrounds: [{stage: 'season12'}]}},
    emergencynotice: {news: {messages: [{title: 'LawinServer V2'}, {title: 'Local maintenance'}]}},
    emergencynoticev2: {emergencynotices: {emergencynotices: [
      {title: 'LawinServer V2'}, {title: 'Local maintenance'}]}}
  }));
  let result;
  handler({}, {json: value => result = value});
  assert.deepEqual(result.playlistinformation.playlist_info.playlists,
    [{playlist_name: 'Playlist_DefaultSolo', image: 'existing-solo-image'}]);
  assert.equal(result.dynamicbackgrounds.backgrounds.backgrounds[0].stage, 'season12');
  assert.deepEqual(result.emergencynotice.news.messages, [{title: 'Local maintenance'}]);
  assert.deepEqual(result.emergencynoticev2.emergencynotices.emergencynotices,
    [{title: 'Local maintenance'}]);
});

test('missing content schema is not replaced with fabricated playlists', () => {
  let handler;
  register({get: (path, callback) => handler = callback}, () => ({title: 'Legacy content'}));
  handler({}, {json: value => assert.deepEqual(value, {title: 'Legacy content'})});
});
