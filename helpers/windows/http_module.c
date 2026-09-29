/* Original diagnostic DLL. Default: lifecycle receipt only. Separately compiled
 * observer: forward one validated HTTP method unchanged and report categories.
 * Neither changes URLs, networking, authentication, security or game files. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef FOL_INPROCESS_OBSERVER
#include "inprocess_observer.h"
#endif

static DWORD WINAPI module_worker(void *unused) {
    (void)unused;
    static wchar_t path[4096];
    wchar_t session[128];
    DWORD path_size = GetEnvironmentVariableW(L"FOL_HTTP_MODULE_REPORT", path, 4096);
    DWORD size = GetEnvironmentVariableW(L"FOL_SESSION_ID", session, 128);
    if (!path_size || path_size >= 4096 || !size || size >= 128)
        return 64;
    char report[384];
    DWORD count = 0;
    const char header[] = "{\"event\":\"module-ready\",\"session_id\":\"";
    for (unsigned i = 0; i < sizeof(header) - 1; ++i)
        report[count++] = header[i];
    for (DWORD i = 0; i < size; ++i) {
        wchar_t c = session[i];
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') ||
              c == L'_' || c == L'-'))
            return 64;
        report[count++] = (char)c;
    }
    const char middle[] = "\",\"process_id\":";
    for (unsigned i = 0; i < sizeof(middle) - 1; ++i)
        report[count++] = middle[i];
    DWORD pid = GetCurrentProcessId(), digits = 0;
    char number[12];
    do {
        number[digits++] = (char)('0' + pid % 10);
        pid /= 10;
    } while (pid);
    while (digits)
        report[count++] = number[--digits];
    report[count++] = '}';
    report[count++] = '\n';
    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 68;
    DWORD written;
    BOOL okay = WriteFile(file, report, count, &written, NULL) && written == count &&
                FlushFileBuffers(file);
    CloseHandle(file);
#ifdef FOL_INPROCESS_OBSERVER
    return okay ? observe_inprocess(path, session) : 68;
#else
    return okay ? 0 : 68;
#endif
}
BOOL WINAPI module_entry(HINSTANCE module, DWORD reason, void *reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(NULL, 0, module_worker, NULL, 0, NULL);
        if (!thread)
            return FALSE;
        CloseHandle(thread);
    }
    return TRUE;
}
