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
      {title: 'LawinServer V2'}, {title: 'Local maintenance'}]}},
    battleroyalenews: {news: {messages: [{title: 'Join my discord server!'},
      {title: 'LawinServer V2'}, {title: 'Patch notes'}]}},
    loginmessage: {loginmessage: {message: {title: 'LawinServer V2', body: 'Join my discord'}}}
  }));
  let result;
  handler({}, {json: value => result = value});
  assert.deepEqual(result.playlistinformation.playlist_info.playlists,
    [{playlist_name: 'Playlist_DefaultSolo', image: 'existing-solo-image'}]);
  assert.equal(result.dynamicbackgrounds.backgrounds.backgrounds[0].stage, 'season12');
  assert.deepEqual(result.emergencynotice.news.messages, [{title: 'Local maintenance'}]);
  assert.deepEqual(result.emergencynoticev2.emergencynotices.emergencynotices,
    [{title: 'Local maintenance'}]);
  assert.deepEqual(result.battleroyalenews.news.messages, [{title: 'Patch notes'}]);
  assert.equal(result.loginmessage, undefined);
});

test('an unrelated login message is kept', () => {
  let handler;
  register({get: (path, callback) => handler = callback}, () => ({
    loginmessage: {loginmessage: {message: {title: 'Maintenance tonight'}}}}));
  handler({}, {json: value => assert.equal(value.loginmessage.loginmessage.message.title,
    'Maintenance tonight')});
});

test('missing content schema is not replaced with fabricated playlists', () => {
  let handler;
  register({get: (path, callback) => handler = callback}, () => ({title: 'Legacy content'}));
  handler({}, {json: value => assert.deepEqual(value, {title: 'Legacy content'})});
});
