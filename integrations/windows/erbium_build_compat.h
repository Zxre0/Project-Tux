// Build-only correction for Erbium@44568e08 SDK/Defs.h:12. That header
// defines an otherwise unused _Inits vector in every translation unit.
// Coalesce the definitions with MSVC's selectany linkage; do not suppress
// arbitrary duplicate-symbol errors or change game hooks/offsets.
#pragma once
// SPDX-License-Identifier: BSD-3-Clause
#define _Inits __declspec(selectany) _Inits
