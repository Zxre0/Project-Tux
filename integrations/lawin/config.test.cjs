// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {REVISION, privateJson, deployment, requireIsolation} = require('./config.cjs');
test('private configuration and credential restrictions', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'fol-lawin-config-'));
  try {
    const file = path.join(dir, 'login.json');
    const local = {username: 'player@localhost', password: 'a'.repeat(32)};
    fs.writeFileSync(file, JSON.stringify(local), {mode: 0o600});
    assert.deepEqual(privateJson(file), local);
    fs.chmodSync(file, 0o644);
    assert.throws(() => privateJson(file), /owner-only/);
    fs.chmodSync(file, 0o600);
    const link = path.join(dir, 'link');
    fs.symlinkSync(file, link);
    assert.throws(() => privateJson(link), /owner-only/);
    const configFile = path.join(dir, 'config.json');
    const config = {revision: REVISION, root: dir, node: process.execPath,
      mongod: '/example/mongod', runner: '/example/session.cjs', login: file};
    fs.writeFileSync(path.join(dir, 'ownership.json'), JSON.stringify({revision: REVISION, root: dir}),
      {mode: 0o600});
    fs.writeFileSync(configFile, JSON.stringify(config), {mode: 0o600});
    assert.equal(deployment(configFile).config.revision, REVISION);
    fs.writeFileSync(path.join(dir, 'ownership.json'), JSON.stringify({revision: REVISION, root: '/wrong'}));
    assert.throws(() => deployment(configFile), /not this deployment/);
    fs.writeFileSync(path.join(dir, 'ownership.json'), JSON.stringify({revision: REVISION, root: dir}));
    fs.writeFileSync(file, JSON.stringify({...local, username: 'someone@example.com'}));
    assert.throws(() => deployment(configFile), /local credentials/);
    fs.writeFileSync(file, JSON.stringify(local));
    fs.writeFileSync(configFile, JSON.stringify({...config, revision: 'unverified'}));
    assert.throws(() => deployment(configFile), /revision/);
    fs.writeFileSync(file, 'x'.repeat(16385));
    assert.throws(() => privateJson(file), /bounded/);
  } finally { fs.rmSync(dir, {recursive: true}); }
});
test('isolation receipt cannot be missing, malformed, or the current namespace', () => {
  const net = fs.statSync('/proc/self/ns/net', {bigint: true});
  for (const value of ['', 'invalid', `${net.dev}:${net.ino}`])
    assert.throws(() => requireIsolation(value), /namespace/);
});
