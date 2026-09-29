// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const REVISION = '2205696e4e1f828b4f6457233901373c07f3e180';
function privateJson(file) {
  const stat = fs.lstatSync(file);
  if (!stat.isFile() || (stat.mode & 0o077) || stat.uid !== process.getuid() || stat.size > 16384)
    throw new Error('Expected bounded owner-only regular configuration');
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}
function deployment(file) {
  const config = privateJson(file);
  if (config.revision !== REVISION) throw new Error('Unrecognized deployment revision');
  for (const name of ['root','node','mongod','login','runner'])
    if (typeof config[name] !== 'string' || !path.isAbsolute(config[name]))
      throw new Error('Deployment paths must be absolute');
  const ownership = privateJson(path.join(config.root, 'ownership.json'));
  if (ownership.revision !== REVISION || ownership.root !== fs.realpathSync(config.root))
    throw new Error('Database directory is not this deployment');
  const login = privateJson(config.login);
  if (Object.keys(login).length !== 2 ||
      !/^[a-z0-9_-]{1,32}@localhost$/.test(login.username) || !/^[a-f0-9]{32}$/.test(login.password))
    throw new Error('Only generated local credentials are accepted');
  return {config, login};
}
function requireIsolation(outer = process.env.FOL_OUTER_NETNS) {
  const net = fs.statSync('/proc/self/ns/net', {bigint: true});
  if (!/^\d+:\d+$/.test(outer || '') || `${net.dev}:${net.ino}` === outer)
    throw new Error('Separate network namespace required');
}
module.exports = {REVISION, privateJson, deployment, requireIsolation};
