// SPDX-License-Identifier: GPL-3.0-only
'use strict';
// Schema evidence only. Never emit arbitrary property names or scalar values.
const fields = new Set(['token','profileToken','profile_token','accountId','signature',
  'data','payload','profile','attributes','request','response','result','status',
  'error','errors','errorCode','error_code','code','reason','version','platform',
  'type','timestamp','message','claims','header','body','metadata','valid']);
function describe(value, isLocalToken = () => false) {
  let remaining = 48;
  function visit(input, depth) {
    --remaining;
    const type = input === null ? 'null' : Array.isArray(input) ? 'array' : typeof input;
    const out = {type};
    if (type === 'string') {
      out.length = input.length;
      out.matches_local_access_token = isLocalToken(input);
    } else if (type === 'object' || type === 'array') {
      const entries = Object.entries(input);
      out.count = entries.length;
      out.children = [];
      for (const [name, child] of entries) {
        if (depth >= 3 || remaining <= 0 || out.children.length >= 16) break;
        out.children.push({field: type === 'array' ? 'element' : fields.has(name) ? name : 'unrecognized',
          ...visit(child, depth + 1)});
      }
      out.truncated = out.children.length !== entries.length;
    }
    return out;
  }
  return visit(value, 0);
}
module.exports = {describe};
