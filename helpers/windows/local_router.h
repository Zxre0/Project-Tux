/* Explicit local-only routing experiment; included by the observer variant.
 * Caller owns both descriptor and scratch buffer. The game setter must copy it;
 * verify the resulting object string before recording a successful local rewrite. */
#include "local_url.h"
typedef struct {
    ULONG_PTR data;
    LONG count, capacity;
} module_string;
typedef void(WINAPI *url_setter)(void *, const module_string *);
static url_setter game_set_url;
static wchar_t routed_url[4096];
static unsigned routed_count;
static BOOL configure_router(ULONG_PTR base, const IMAGE_DOS_HEADER *dos,
                             const IMAGE_NT_HEADERS64 *pe, BOOL fixture) {
    DWORD function = 0, slot = 0;
    if (fixture) {
        ULONG_PTR setter = (ULONG_PTR)GetProcAddress((HMODULE)base, "synthetic_set_url");
        if (!setter || setter < base || setter - base >= pe->OptionalHeader.SizeOfImage)
            return FALSE;
        function = (DWORD)(setter - base);
        slot = 1;
    } else {
        wchar_t text[64];
        DWORD size = GetEnvironmentVariableW(L"FOL_HTTP_ROUTER_LAYOUT", text, 64);
        if (!size || size >= 64)
            return FALSE;
        DWORD fields[2];
        const wchar_t *p = text;
        for (unsigned i = 0; i < 2; ++i) {
            DWORD value = 0;
            if (*p < L'0' || *p > L'9')
                return FALSE;
            while (*p >= L'0' && *p <= L'9') {
                if (value > 26843545)
                    return FALSE;
                value = value * 10 + (DWORD)(*p++ - L'0');
            }
            fields[i] = value;
            if (i == 0 ? *p++ != L',' : *p != 0)
                return FALSE;
        }
        function = fields[0];
        slot = fields[1];
    }
    if (slot >= 64 || function >= pe->OptionalHeader.SizeOfImage ||
        !section_role(base, dos, pe, function, 1, TRUE) ||
        !section_role(base, dos, pe, (DWORD)(expected_table - base) + slot * 8, 8, FALSE))
        return FALSE;
    ULONG_PTR target;
    if (!module_read(expected_table + slot * 8, &target, sizeof(target)) ||
        target != base + function)
        return FALSE;
    game_set_url = (url_setter)target;
    return TRUE;
}
static BOOL route_request(void *object) {
    int selected = fol_local_url(observed_url, routed_url, 4096);
    if (selected < 0)
        return FALSE;
    if (selected == 0)
        return TRUE;
    LONG size = 0;
    while (routed_url[size])
        ++size;
    ++size;
    const module_string replacement = {(ULONG_PTR)routed_url, size, size};
    game_set_url(object, &replacement);
    module_string after;
    if (!module_read((ULONG_PTR)object + string_offset, &after, sizeof(after)) ||
        after.count != size || after.capacity < size ||
        !module_read(after.data, observed_url, (SIZE_T)size * sizeof(wchar_t)))
        return FALSE;
    for (LONG i = 0; i < size; ++i)
        if (observed_url[i] != routed_url[i])
            return FALSE;
    ++routed_count;
    return TRUE;
}
