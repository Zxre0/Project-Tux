// SPDX-License-Identifier: GPL-3.0-only
// Local XMPP session subset: framing, SASL, binding, session, roster and ping.
// Protocol reference: LawinServerV2 2205696 xmpp/xmpp.js; no matchmaker listener.
'use strict';
const crypto = require('node:crypto');
const framing = 'urn:ietf:params:xml:ns:xmpp-framing';
const sasl = 'urn:ietf:params:xml:ns:xmpp-sasl';
const domain = 'prod.ol.epicgames.com'; // JID namespace only; never a network destination.
const escape = value => String(value).replace(/[<>&"']/g,
  c => ({'<':'&lt;','>':'&gt;','&':'&amp;','"':'&quot;',"'":'&apos;'}[c]));
function session({send, close, authenticate, record = () => {}}) {
  let state = 'new', identity, jid;
  function fail() {
    if (state === 'closed') return;
    send(`<failure xmlns="${sasl}"><not-authorized/></failure>`);
    state = 'closed'; close(); record({event: 'xmpp-rejected'});
  }
  async function handle(root) {
    if (state === 'closed') return;
    const attrs = root?.attributes || {}, children = root?.children || [];
    if (root?.name === 'close') { state = 'closed'; close(); return; }
    if (root?.name === 'open') {
      if (!['new','authenticated'].includes(state) || attrs.xmlns !== framing) return fail();
      send(`<open xmlns="${framing}" from="${domain}" id="${crypto.randomUUID()}" version="1.0"/>`);
      const feature = state === 'new'
        ? `<mechanisms xmlns="${sasl}"><mechanism>PLAIN</mechanism></mechanisms>`
        : '<bind xmlns="urn:ietf:params:xml:ns:xmpp-bind"/><session xmlns="urn:ietf:params:xml:ns:xmpp-session"/>';
      send(`<stream:features xmlns:stream="http://etherx.jabber.org/streams">${feature}</stream:features>`);
      state = state === 'new' ? 'opened' : 'binding'; return;
    }
    if (root?.name === 'auth') {
      if (state !== 'opened' || attrs.mechanism !== 'PLAIN' || attrs.xmlns !== sasl ||
          typeof root.content !== 'string' || root.content.length > 8192 ||
          !/^[A-Za-z0-9+/]+={0,2}$/.test(root.content)) return fail();
      const fields = Buffer.from(root.content, 'base64').toString('utf8').split('\0');
      if (fields.length !== 3 || (fields[0] && fields[0] !== fields[1])) return fail();
      identity = await authenticate(fields[1], fields[2]);
      if (!identity || identity !== fields[1] || !/^[a-f0-9]{32}$/.test(identity)) return fail();
      state = 'authenticated'; send(`<success xmlns="${sasl}"/>`);
      record({event: 'xmpp-authenticated'}); return;
    }
    if (root?.name === 'iq') {
      if (!['binding','bound'].includes(state) || typeof attrs.id !== 'string' ||
          attrs.id.length > 256 || !['get','set'].includes(attrs.type)) return fail();
      const child = children[0];
      const reply = body => send(`<iq xmlns="jabber:client" type="result" id="${escape(attrs.id)}"${jid ? ` to="${escape(jid)}"` : ''}>${body}</iq>`);
      if (child?.name === 'bind' && child.attributes?.xmlns === 'urn:ietf:params:xml:ns:xmpp-bind' && state === 'binding') {
        const resource = child.children?.find(c => c.name === 'resource')?.content;
        if (typeof resource !== 'string' || !resource || resource.length > 512 || /[\x00-\x1f]/.test(resource)) return fail();
        jid = `${identity}@${domain}/${resource}`; state = 'bound';
        reply(`<bind xmlns="urn:ietf:params:xml:ns:xmpp-bind"><jid>${escape(jid)}</jid></bind>`);
        record({event: 'xmpp-bound'}); return;
      }
      if (state !== 'bound') return fail();
      if (child?.name === 'session' && child.attributes?.xmlns === 'urn:ietf:params:xml:ns:xmpp-session') {
        reply(''); record({event: 'xmpp-session-ready'}); return;
      }
      if (child?.name === 'ping' && child.attributes?.xmlns === 'urn:xmpp:ping') { reply(''); return; }
      if (child?.name === 'query' && child.attributes?.xmlns === 'jabber:iq:roster') {
        // This adapter is explicitly single-account; there are no other local contacts.
        reply('<query xmlns="jabber:iq:roster"/>'); return;
      }
      send(`<iq xmlns="jabber:client" type="error" id="${escape(attrs.id)}"><error type="cancel"><service-unavailable xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/></error></iq>`);
      record({event: 'xmpp-unsupported-iq'}); return;
    }
    if (root?.name === 'presence' && state === 'bound' && !attrs.to) {
      record({event: 'xmpp-presence'}); return;
    }
    // No fabricated room membership or remote message delivery.
    fail();
  }
  return {handle, fail};
}
async function listen({upstream, authenticate, record}) {
  const {WebSocketServer} = upstream('ws');
  const parse = upstream('xml-parser');
  const server = new WebSocketServer({host: '127.0.0.1', port: 8080,
    maxPayload: 32768, perMessageDeflate: false,
    handleProtocols: protocols => protocols.has('xmpp') ? 'xmpp' : false});
  server.on('connection', socket => {
    if (socket.protocol !== 'xmpp' || server.clients.size > 4) { socket.close(1008); return; }
    const peer = session({send: data => { if (socket.readyState === 1) socket.send(data); },
      close: () => socket.close(1008), authenticate, record});
    let pending = 0, queue = Promise.resolve();
    socket.on('message', (data, binary) => {
      if (binary || ++pending > 16) { socket.close(1008); return; }
      queue = queue.then(async () => {
        const text = data.toString('utf8');
        if (/<!DOCTYPE|<!ENTITY/i.test(text)) return peer.fail();
        await peer.handle(parse(text)?.root);
      }).catch(() => { record({event: 'xmpp-handler-failed'}); peer.fail(); })
        .finally(() => --pending);
    });
    socket.on('error', () => record({event: 'xmpp-socket-error'}));
    socket.on('close', () => record({event: 'xmpp-disconnected'}));
  });
  await new Promise((resolve, reject) => { server.once('listening', resolve); server.once('error', reject); });
  server.on('error', () => record({event: 'xmpp-server-error'}));
  record({event: 'xmpp-listening', address: '127.0.0.1', port: 8080});
  return server;
}
module.exports = {session, listen};
