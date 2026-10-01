// Build-only correction for Erbium@44568e08 SDK/Defs.h:12. That header
// defines an otherwise unused _Inits vector in every translation unit.
// Coalesce the definitions with MSVC's selectany linkage; do not suppress
// arbitrary duplicate-symbol errors or change game hooks/offsets.
#pragma once
// SPDX-License-Identifier: BSD-3-Clause
#define _Inits __declspec(selectany) _Inits

// Hide the console Erbium allocates when its GUI is disabled: under Proton it
// is a separate top-level window, and closing it terminates the game server.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
inline BOOL fol_alloc_hidden_console() {
    const BOOL created = ::AllocConsole();
    if (created && GetConsoleWindow())
        ShowWindow(GetConsoleWindow(), SW_HIDE);
    return created;
}
#define AllocConsole fol_alloc_hidden_console
