// SPDX-License-Identifier: GPL-3.0-only
// Original single-account, session-local private party state. No matchmaking.
// Wire shape reference: Reboot-Launcher auth_backend/structure/party.js.
'use strict';
const crypto = require('node:crypto');
function metadata(value = {}) {
  if (!value || typeof value !== 'object' || Array.isArray(value) ||
      Object.keys(value).length > 128 || JSON.stringify(value).length > 16384 ||
      Object.entries(value).some(([k,v]) => ['__proto__','constructor','prototype'].includes(k) ||
        k.length > 256 || typeof v !== 'string')) throw new Error('invalid_metadata');
  return structuredClone(value);
}
function store(accountId) {
  let party;
  const copy = () => structuredClone(party);
  const now = () => new Date().toISOString();
  function requireParty(id) { if (!party || party.id !== id) throw new Error('party_not_found'); }
  function patchMeta(current, update = {}, remove = []) {
    if (!Array.isArray(remove) || remove.length > 128 || !remove.every(k => typeof k === 'string'))
      throw new Error('invalid_metadata');
    const merged = {...current, ...metadata(update)};
    for (const key of remove) delete merged[key];
    return metadata(merged);
  }
  return {
    summary: () => ({current: party ? [copy()] : [], pending: [], invites: [], pings: []}),
    get: id => { requireParty(id); return copy(); },
    create(body) {
      if (party) throw new Error('party_already_exists');
      const connection = body?.join_info?.connection;
      if (!connection || typeof connection.id !== 'string' || connection.id.length > 768 ||
          !connection.id.startsWith(accountId + '@prod.ol.epicgames.com/') ||
          /[\x00-\x1f]/.test(connection.id)) throw new Error('invalid_connection');
      const config = body.config || {};
      if (!Number.isInteger(config.max_size) || config.max_size < 1 || config.max_size > 16)
        throw new Error('invalid_config');
      const stamp = now();
      party = {id: crypto.randomUUID().replaceAll('-', ''), created_at: stamp, updated_at: stamp,
        config: {type: 'DEFAULT', sub_type: 'default', joinability: 'INVITE_AND_FORMER',
          discoverability: 'INVITED_ONLY', max_size: config.max_size, invite_ttl: 14400, intention_ttl: 60},
        members: [{account_id: accountId, meta: metadata(body.join_info.meta),
          connections: [{id: connection.id, connected_at: stamp, updated_at: stamp,
            yield_leadership: false, meta: metadata(connection.meta)}],
          revision: 0, updated_at: stamp, joined_at: stamp, role: 'CAPTAIN'}],
        applicants: [], meta: metadata(body.meta), invites: [], revision: 0, intentions: []};
      return copy();
    },
    patch(id, body, member = false) {
      requireParty(id);
      const target = member ? party.members[0] : party;
      if (!Number.isInteger(body?.revision) || body.revision !== target.revision)
        throw new Error('revision_conflict');
      if (body.meta !== undefined && (!body.meta || typeof body.meta !== 'object' || Array.isArray(body.meta)))
        throw new Error('invalid_metadata');
      let config = party.config;
      if (body.config !== undefined) {
        if (member || !body.config || typeof body.config !== 'object' || Array.isArray(body.config))
          throw new Error('invalid_config');
        config = {...party.config};
        for (const [key, value] of Object.entries(body.config)) {
          if (key === 'max_size' && Number.isInteger(value) && value >= 1 && value <= 16)
            config.max_size = value;
          else if (Object.hasOwn(config, key) && config[key] === value) continue;
          else throw new Error('unsupported_config');
        }
      }
      const updated = patchMeta(target.meta, body.meta?.update, body.meta?.delete);
      target.meta = updated; target.revision++; target.updated_at = now();
      party.config = config;
      party.updated_at = now(); return copy();
    },
    remove(id) { requireParty(id); party = undefined; }
  };
}
function register(app, {accountId, authenticate, record}) {
  const state = store(accountId), base = '/party/api/v1/Fortnite';
  app.use(base, async (req, res, next) => {
    const bearer = req.headers.authorization || '';
    if (!bearer.startsWith('bearer ') || !await authenticate(accountId, bearer.slice(7)))
      return res.sendStatus(401);
    next();
  });
  const respond = action => (req, res) => {
    try { action(req, res); }
    catch (err) {
      const codes = {party_not_found: 404, party_already_exists: 409, revision_conflict: 409,
        invalid_metadata: 400, invalid_connection: 400, invalid_config: 400, unsupported_config: 501};
      const code = Object.hasOwn(codes, err.message) ? err.message : 'internal_error';
      record({event: 'party-request-rejected', code});
      res.status(codes[code] || 500).json({errorCode: 'fol.party.' + code, errorMessage: code});
    }
  };
  app.get(base + '/user/:accountId', (req, res) => req.params.accountId === accountId
    ? res.json(state.summary()) : res.sendStatus(403));
  app.post(base + '/parties', respond((req, res) => {
    const value = state.create(req.body); record({event: 'party-created'}); res.json(value);
  }));
  app.get(base + '/parties/:partyId', respond((req, res) => res.json(state.get(req.params.partyId))));
  app.patch(base + '/parties/:partyId', respond((req, res) => {
    state.patch(req.params.partyId, req.body); record({event: 'party-updated'}); res.sendStatus(204);
  }));
  app.patch(base + '/parties/:partyId/members/:accountId/meta', respond((req, res) => {
    if (req.params.accountId !== accountId) return res.sendStatus(403);
    // Member metadata is flattened in the member API, unlike party metadata.
    state.patch(req.params.partyId, {revision: req.body?.revision,
      meta: {update: req.body?.update, delete: req.body?.delete}}, true);
    record({event: 'party-member-updated'}); res.sendStatus(204);
  }));
  app.delete(base + '/parties/:partyId/members/:accountId', respond((req, res) => {
    if (req.params.accountId !== accountId) return res.sendStatus(403);
    state.remove(req.params.partyId); record({event: 'party-left'}); res.sendStatus(204);
  }));
  app.use(base, (req, res) => res.sendStatus(501));
}
module.exports = {store, register};
