// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const {EventEmitter} = require('node:events');
const {preserveSessions} = require('./multi-session.cjs');

test('a second login keeps the first session of the same account', () => {
  const access = [{accountId: 'a', token: 'server'}], refresh = [{accountId: 'a', token: 'r1'}];
  const res = new EventEmitter();
  preserveSessions(() => [access, refresh])({}, res, () => {
    access.splice(0, 1); refresh.splice(0, 1); // upstream revocation
    access.push({accountId: 'a', token: 'client'}); refresh.push({accountId: 'a', token: 'r2'});
  });
  res.emit('finish');
  assert.deepEqual(access.map(e => e.token).sort(), ['client', 'server']);
  assert.deepEqual(refresh.map(e => e.token).sort(), ['r1', 'r2']);
});
