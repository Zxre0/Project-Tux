/* Asset-free Windows child for read-only HTTP interface discovery tests. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

__declspec(noinline) void synthetic_request(void) {
    GetModuleHandleW(L"STAT_FCurlHttpRequest_ProcessRequest");
    Sleep(1);
}
__declspec(noinline) void synthetic_other(void) { Sleep(2); }
void (*volatile synthetic_table[])(void) = {synthetic_request, synthetic_other};
void fixture_entry(void) {
    synthetic_table[0]();
    for (;;)
        Sleep(1000);
}
