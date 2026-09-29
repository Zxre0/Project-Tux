/* Asset-free Windows child for read-only HTTP interface discovery tests. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

__declspec(noinline) BYTE synthetic_request(void *object) {
    GetModuleHandleW(L"STAT_FCurlHttpRequest_ProcessRequest");
    if (object)
        Sleep(1);
    return 1;
}
__declspec(noinline) BYTE synthetic_other(void *object) {
    if (object)
        Sleep(2);
    return 1;
}
__declspec(dllexport) BYTE (*volatile synthetic_table[])(void *) = {
    synthetic_request,
    synthetic_other
};
static wchar_t synthetic_url[] =
    L"http://127.0.0.1:3551/account/api/oauth/token?fixture-secret=never-log-this";
struct fixture_request {
    void *table;
    BYTE padding[96];
    struct {
        wchar_t *data;
        LONG count, capacity;
    } url;
};
_Static_assert(__builtin_offsetof(struct fixture_request, url) == 104, "fixture URL layout");
static struct fixture_request request = {(void *)synthetic_table,
                                         {0},
                                         {synthetic_url, sizeof(synthetic_url) / sizeof(wchar_t),
                                          sizeof(synthetic_url) / sizeof(wchar_t)}};
void fixture_entry(void) {
    for (;;) {
        if (synthetic_table[0](&request) != 1)
            ExitProcess(69);
        Sleep(250);
    }
}
