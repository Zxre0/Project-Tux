// SPDX-License-Identifier: GPL-3.0-only
// Shared accounts (Multiplayer > Join, "Use the server's accounts"): instead
// of a local Lawin, forward the game's loopback backend ports to the backend of
// the server being joined (Project Tux Windows Server). The local router DLL
// still sends the game to 127.0.0.1, so nothing in the game changes; inside
// the sandbox only the server's address is routable.
//
//   node relay.cjs REMOTE_CONFIG LOG PROGRAM [ARGS...]   (same shape as session.cjs)
//
// REMOTE_CONFIG (owner-only, written by scripts/fortnite-match.sh):
//   {"node", "runner", "server": "192.168.1.20", "username", "password"}
'use strict';
const fs = require('node:fs');
const net = require('node:net');
const http = require('node:http');
const path = require('node:path');
const { spawn } = require('node:child_process');
const {privateJson, requireIsolation} = require('./config.cjs');

const PORTS = [3551, 3552, 8080]; // backend, matchmaker, XMPP
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

function relayConfig(value) {
  const ipv4 = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/.exec(value?.server || '');
  if (!ipv4 || ipv4.slice(1).some(octet => Number(octet) > 255) || ipv4[1] === '127')
    throw new Error('Remote backend needs the server\'s IPv4 address');
  for (const key of ['username', 'password'])
    if (typeof value[key] !== 'string' || !value[key] || value[key].length > 128 || /[\r\n\0]/.test(value[key]))
      throw new Error('Remote backend needs a login and password');
  return value;
}

// One loopback listener per port, each connection piped to the same port on the server.
function forward(port, server, record, {listenPort = port, targetPort = port} = {}) {
  // allowHalfOpen: a client that finishes sending still gets the server's reply.
  const listener = net.createServer({allowHalfOpen: true}, client => {
    const upstream = net.connect({port: targetPort, host: server, allowHalfOpen: true});
    const close = () => { client.destroy(); upstream.destroy(); };
    client.on('error', close);
    upstream.on('error', () => { record({event: 'relay-connect-failed', port}); close(); });
    client.pipe(upstream).pipe(client);
  });
  return new Promise((resolve, reject) => {
    listener.once('error', reject);
    listener.listen(listenPort, '127.0.0.1', () => resolve(listener));
  });
}

// The server's backend answers its health route only for the loopback Host the game uses.
function healthy(server, port = 3551) {
  return new Promise(resolve => {
    const request = http.get({host: server, port, path: '/fol/health', timeout: 3000, agent: false,
      headers: {Host: '127.0.0.1:3551'}}, response => {
      let body = '';
      response.on('data', chunk => body += chunk);
      response.on('end', () => {
        try { resolve(response.statusCode === 200 && JSON.parse(body).ready === true); }
        catch { resolve(false); }
      });
    });
    request.on('timeout', () => request.destroy());
    request.on('error', () => resolve(false));
  });
}

async function main() {
  const [configFile, logFile, program, ...args] = process.argv.slice(2);
  const config = relayConfig(privateJson(configFile));
  requireIsolation();
  if (!program || !path.isAbsolute(program)) throw new Error('Absolute child executable required');
  const log = fs.openSync(logFile, 'wx', 0o600);
  const record = data => fs.writeSync(log, JSON.stringify({timestamp: new Date().toISOString(),
    component: 'relay', session_id: process.env.FOL_SESSION_ID, ...data}) + '\n');
  // The sandbox's network is attached from outside right after it starts.
  let up = false;
  for (let i = 0; i < 60 && !(up = await healthy(config.server)); ++i) await delay(1000);
  if (!up) {
    record({event: 'remote-backend-unreachable'});
    console.error(`Backend: cannot reach the server's accounts at ${config.server}:3551 ` +
      '(is it running with shared accounts on, and are you on its LAN/VPN?)');
    process.exit(75);
  }
  const listeners = await Promise.all(PORTS.map(port => forward(port, config.server, record)));
  record({event: 'relay-ready', server: config.server});
  console.log(`Using the accounts on ${config.server} (signed in as ${config.username})`);
  const game = spawn(program, args, {stdio: 'inherit'});
  record({event: 'process-start', name: 'game', pid: game.pid});
  for (const signal of ['SIGTERM', 'SIGINT']) process.on(signal, () => game.kill(signal));
  game.on('exit', (code, signal) => {
    record({event: 'process-exit', name: 'game', code, signal});
    for (const listener of listeners) listener.close();
    fs.closeSync(log);
    process.exit(code ?? 70);
  });
}

if (require.main === module)
  main().catch(err => { console.error(`Backend: ${err.message}`); process.exit(2); });
module.exports = {relayConfig, forward, healthy, PORTS};
