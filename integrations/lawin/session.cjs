// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const { spawn, fork } = require('node:child_process');
const { once } = require('node:events');
const {deployment, requireIsolation} = require('./config.cjs');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function main() {
  process.umask(0o077);
  const [configFile, logFile, program, ...args] = process.argv.slice(2);
  const {config} = deployment(configFile);
  requireIsolation();
  if (!program || !path.isAbsolute(program)) throw new Error('Absolute child executable required');
  const lock = path.join(config.root, 'session.lock');
  fs.mkdirSync(lock); // Never guess that an existing lock is stale or steal another session's DB.
  let log;
  try { log = fs.openSync(logFile, 'wx', 0o600); }
  catch (err) { fs.rmdirSync(lock); throw err; }
  const record = data => fs.writeSync(log, JSON.stringify({timestamp: new Date().toISOString(),
    component: 'lawin', session_id: process.env.FOL_SESSION_ID, ...data}) + '\n');
  const children = [];
  let stopping = false;
  async function stop(code) {
    if (stopping) return;
    stopping = true;
    for (const child of [...children].reverse()) {
      if (child.exitCode === null && child.signalCode === null) {
        child.kill('SIGTERM');
        await Promise.race([once(child, 'exit'), delay(3000)]);
        if (child.exitCode === null && child.signalCode === null) {
          child.kill('SIGKILL');
          await once(child, 'exit');
        }
      }
    }
    record({event: 'services-stopped', code});
    fs.closeSync(log);
    fs.rmdirSync(lock);
    process.exit(code);
  }
  for (const signal of ['SIGTERM','SIGINT']) process.on(signal, () => stop(128));
  function own(child, name) {
    children.push(child);
    record({event: 'process-start', name, pid: child.pid});
    child.on('error', () => { record({event: 'spawn-failed', name}); stop(71); });
    child.on('exit', (code, signal) => {
      record({event: 'process-exit', name, code, signal});
      if (!stopping) stop(name === 'game' ? (code ?? 70) : 72);
    });
    return child;
  }
  try {
    const mongoLog = fs.openSync(path.join(path.dirname(logFile), 'mongodb.log'), 'wx', 0o600);
    own(spawn(config.mongod, ['--dbpath', path.join(config.root, 'database'), '--bind_ip',
      '127.0.0.1', '--port', '27017', '--nounixsocket', '--wiredTigerCacheSizeGB', '0.25'],
      {stdio: ['ignore', mongoLog, mongoLog]}), 'mongodb');
    fs.closeSync(mongoLog);
    const serviceLog = fs.openSync(path.join(path.dirname(logFile), 'lawin-service.log'), 'wx', 0o600);
    const service = own(fork(path.join(__dirname, 'service.cjs'), [configFile],
      {stdio: ['ignore', serviceLog, serviceLog, 'ipc']}), 'backend');
    fs.closeSync(serviceLog);
    const ready = new Promise((resolve, reject) => {
      service.on('message', data => { record(data); if (data.event === 'ready') resolve(); });
      service.once('exit', () => reject(new Error('Backend exited before ready')));
    });
    await Promise.race([ready, delay(30000).then(() => { throw new Error('Backend readiness timeout'); })]);
    record({event: 'starting-game'});
    own(spawn(program, args, {stdio: 'inherit'}), 'game');
  } catch {
    record({event: 'session-setup-failed'});
    await stop(73);
  }
}
main().catch(() => { console.error('Lawin session setup failed; inspect private configuration/logs'); process.exit(2); });
