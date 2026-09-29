/* Asset-free Windows child for read-only HTTP interface discovery tests. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

__declspec(noinline) BYTE synthetic_request(void *object) {
    GetModuleHandleW(L"STAT_FCurlHttpRequest_ProcessRequest");
    if (object)
        Sleep(1);
    return 1;
}
__declspec(dllexport) void synthetic_set_url(void *object, const void *value);
__declspec(dllexport) void *volatile synthetic_table[] = {(void *)synthetic_request,
                                                          (void *)synthetic_set_url};
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
__declspec(dllexport) __declspec(noinline) void synthetic_set_url(void *object, const void *value) {
    static wchar_t owned_url[4096];
    const typeof(request.url) *source = value;
    if (object != &request || source->count < 1 || source->count > 4096 ||
        source->capacity < source->count || !source->data || source->data[source->count - 1])
        ExitProcess(70);
    for (LONG i = 0; i < source->count; ++i)
        owned_url[i] = source->data[i];
    request.url.data = owned_url;
    request.url.count = source->count;
    request.url.capacity = 4096;
}
static BOOL fixture_argument(const wchar_t *needle) {
    const wchar_t *line = GetCommandLineW();
    for (unsigned i = 0; line[i]; ++i) {
        unsigned j = 0;
        while (needle[j] && line[i + j] == needle[j])
            ++j;
        if (!needle[j])
            return TRUE;
    }
    return FALSE;
}
void fixture_entry(void) {
    wchar_t flag[2];
    if (GetEnvironmentVariableW(L"FOL_HTTP_FIXTURE_LOGIN", flag, 2) == 1 && flag[0] == L'1') {
        if (!fixture_argument(L" -AUTH_LOGIN=fixture@localhost") ||
            !fixture_argument(L" -AUTH_PASSWORD=0123456789abcdef0123456789abcdef") ||
            !fixture_argument(L" -AUTH_TYPE=epic"))
            ExitProcess(71);
    }
    if (GetEnvironmentVariableW(L"FOL_HTTP_FIXTURE_ROUTE", flag, 2) == 1 && flag[0] == L'1') {
        static wchar_t source[] = L"https://account-public-service.ol.epicgames.com/account/api/"
                                  L"oauth/token?fixture-secret=never-log-this";
        request.url.data = source;
        request.url.count = request.url.capacity = sizeof(source) / sizeof(wchar_t);
    }
    for (;;) {
        if (((BYTE(WINAPI *)(void *))synthetic_table[0])(&request) != 1)
            ExitProcess(69);
        Sleep(250);
    }
}
