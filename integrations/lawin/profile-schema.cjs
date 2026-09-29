// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const profileIds = ['common_core', 'common_public', 'athena', 'campaign', 'creative',
  'profile0', 'metadata', 'collections', 'theater0', 'outpost0',
  'collection_book_people0', 'collection_book_schematics0'];
function missingInventories(profiles) {
  if (!profiles || typeof profiles !== 'object' || Array.isArray(profiles))
    throw new Error('Invalid local profile document');
  return profileIds.filter(id => {
    const profile = profiles[id];
    if (profile === undefined) return false;
    if (!profile || typeof profile !== 'object' || Array.isArray(profile))
      throw new Error('Invalid local profile');
    if (Object.hasOwn(profile, 'items') && (!profile.items || typeof profile.items !== 'object' || Array.isArray(profile.items)))
      throw new Error('Invalid existing inventory; not overwritten');
    return !Object.hasOwn(profile, 'items');
  });
}
async function repairEmptyInventories(Profile, accountId) {
  // Retain deliberately empty local inventories through save/toObject. Upstream
  // Mongoose's default minimization otherwise drops this required wire field.
  Profile.schema.set('minimize', false);
  const document = await Profile.findOne({accountId}).lean();
  if (!document) throw new Error('Missing local profile document');
  let repaired = 0;
  for (const id of missingInventories(document.profiles)) {
    const field = `profiles.${id}.items`;
    const result = await Profile.updateOne({accountId, [field]: {$exists: false}},
      {$set: {[field]: {}}});
    repaired += result.modifiedCount;
  }
  return repaired;
}
module.exports = {missingInventories, repairEmptyInventories};
