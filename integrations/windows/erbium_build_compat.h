// Build-only correction for Erbium@44568e08 SDK/Defs.h:12. That header
// defines an otherwise unused _Inits vector in every translation unit.
// Coalesce the definitions with MSVC's selectany linkage; do not suppress
// arbitrary duplicate-symbol errors or change game hooks/offsets.
#pragma once
// SPDX-License-Identifier: BSD-3-Clause
#define _Inits __declspec(selectany) _Inits

// No console window at all: Erbium allocates one when its GUI is disabled,
// and under Proton it flashes up as a separate top-level window (closing it
// kills the server). Its output goes to FOL_ERBIUM_LOG instead, unbuffered.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
inline BOOL fol_alloc_console() { return TRUE; }
#define AllocConsole fol_alloc_console
inline errno_t fol_freopen_s(FILE **file, const char *name, const char *mode, FILE *stream) {
    char log[1024];
    const DWORD size = GetEnvironmentVariableA("FOL_ERBIUM_LOG", log, sizeof(log));
    const bool console = name && (!_stricmp(name, "CONOUT$") || !_stricmp(name, "CONIN$"));
    if (console)
        name = (!_stricmp(name, "CONOUT$") && size && size < sizeof(log)) ? log : "NUL";
    if (console && stream == stderr)
        mode = "a"; // stdout already truncated the log; append stderr to it
    const errno_t result = (freopen_s)(file, name, mode, stream);
    if (!result && console && stream != stdin)
        setvbuf(stream, nullptr, _IONBF, 0);
    return result;
}
#define freopen_s(file, name, mode, stream) fol_freopen_s(file, name, mode, stream)
