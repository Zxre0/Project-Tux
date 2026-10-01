/* Match-hosting launcher: start one Fortnite process, load an optional early
 * DLL (local backend router) shortly after start, then optionally wait for a
 * log marker and load a late DLL (game server). Exits with the child's code.
 *
 *   fol-inject.exe C:\path\FortniteClient-Win64-Shipping.exe ARGS...
 *
 * Environment (Windows paths):
 *   FOL_INJECT_EARLY   DLL loaded ~3 s after the child starts
 *   FOL_INJECT_LATE    DLL loaded after FOL_INJECT_MARKER appears in FOL_INJECT_LOG
 *   FOL_INJECT_LOG     game log to watch (deleted beforehand by the caller)
 *   FOL_INJECT_MARKER  ASCII text to wait for
 *   FOL_INJECT_DELAY   extra milliseconds after the marker (default 5000)
 *   FOL_INJECT_STATUS  optional file that receives the progress lines
 */
#define WIN32_LEAN_AND_MEAN
#define _ACRTIMP
#include <windows.h>

#include <tlhelp32.h>

void *memset(void *destination, int value, SIZE_T size) {
    volatile BYTE *bytes = destination;
    while (size--)
        *bytes++ = (BYTE)value;
    return destination;
}

static HANDLE status = INVALID_HANDLE_VALUE;
static void say(const char *text) {
    DWORD n = 0, written;
    while (text[n])
        ++n;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), text, n, &written, NULL);
    if (status != INVALID_HANDLE_VALUE)
        WriteFile(status, text, n, &written, NULL);
}
static void say_number(DWORD value) {
    char digits[12];
    int n = 11;
    digits[n] = 0;
    do {
        digits[--n] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    say(digits + n);
}
static BOOL equal_ci(const wchar_t *a, const wchar_t *b) {
    for (;; ++a, ++b) {
        wchar_t x = *a >= L'A' && *a <= L'Z' ? *a + 32 : *a;
        wchar_t y = *b >= L'A' && *b <= L'Z' ? *b + 32 : *b;
        if (x != y)
            return FALSE;
        if (!x)
            return TRUE;
    }
}
static const wchar_t *base_name(const wchar_t *path) {
    const wchar_t *name = path;
    for (; *path; ++path)
        if (*path == L'\\' || *path == L'/')
            name = path + 1;
    return name;
}
static ULONG_PTR image_base(DWORD pid, const wchar_t *name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;
    MODULEENTRY32W item = {0};
    item.dwSize = sizeof(item);
    ULONG_PTR base = 0;
    if (Module32FirstW(snapshot, &item)) {
        do {
            if (equal_ci(item.szModule, name)) {
                base = (ULONG_PTR)item.modBaseAddr;
                break;
            }
        } while (Module32NextW(snapshot, &item));
    }
    CloseHandle(snapshot);
    return base;
}

static BOOL inject(PROCESS_INFORMATION *child, const wchar_t *path) {
    say("fol-inject: loading ");
    for (const wchar_t *p = base_name(path); *p; ++p) {
        char c[2] = {(char)*p, 0};
        say(c);
    }
    say("\n");
    FARPROC loader = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HMODULE containing;
    wchar_t containing_path[MAX_PATH];
    if (!loader ||
        !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)loader, &containing) ||
        !GetModuleFileNameW(containing, containing_path, MAX_PATH))
        return FALSE;
    /* LoadLibraryW may be forwarded to KernelBase; resolve it in the child. */
    ULONG_PTR remote_module = image_base(child->dwProcessId, base_name(containing_path));
    if (!remote_module) {
        say("fol-inject: loader module not found in child\n");
        return FALSE;
    }
    ULONG_PTR remote_loader = remote_module + (ULONG_PTR)loader - (ULONG_PTR)containing;
    SIZE_T count = 0;
    while (path[count])
        ++count;
    SIZE_T bytes = (count + 1) * sizeof(wchar_t), written = 0;
    void *remote_path =
        VirtualAllocEx(child->hProcess, NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_path ||
        !WriteProcessMemory(child->hProcess, remote_path, path, bytes, &written) ||
        written != bytes)
        return FALSE;
    HANDLE thread = CreateRemoteThread(child->hProcess, NULL, 0,
                                       (LPTHREAD_START_ROUTINE)remote_loader, remote_path, 0, NULL);
    if (!thread)
        return FALSE;
    DWORD waited = WaitForSingleObject(thread, 60000);
    DWORD result = 0;
    GetExitCodeThread(thread, &result);
    CloseHandle(thread);
    BOOL loaded = waited == WAIT_OBJECT_0 && image_base(child->dwProcessId, base_name(path));
    say(loaded ? "fol-inject: loaded\n" : "fol-inject: load FAILED\n");
    return loaded;
}

/* Poll a growing log for an ASCII marker; returns FALSE if the child exits first. */
static BOOL wait_for_marker(HANDLE process, const wchar_t *log, const char *marker) {
    unsigned marker_size = 0;
    while (marker[marker_size])
        ++marker_size;
    static char buffer[65536 + 512];
    LONGLONG offset = 0;
    unsigned carry = 0;
    for (;;) {
        if (WaitForSingleObject(process, 1000) != WAIT_TIMEOUT)
            return FALSE;
        HANDLE file = CreateFileW(log, GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE)
            continue;
        LARGE_INTEGER position;
        position.QuadPart = offset;
        DWORD got = 0;
        while (SetFilePointerEx(file, position, NULL, FILE_BEGIN) &&
               ReadFile(file, buffer + carry, 65536, &got, NULL) && got) {
            unsigned total = carry + got;
            for (unsigned i = 0; i + marker_size <= total; ++i) {
                unsigned j = 0;
                while (j < marker_size && buffer[i + j] == marker[j])
                    ++j;
                if (j == marker_size) {
                    CloseHandle(file);
                    return TRUE;
                }
            }
            carry = total < marker_size ? total : marker_size - 1;
            for (unsigned i = 0; i < carry; ++i)
                buffer[i] = buffer[total - carry + i];
            offset += got;
            position.QuadPart = offset;
        }
        CloseHandle(file);
    }
}

static wchar_t *skip_argument(wchar_t *line, wchar_t *word) {
    while (*line == L' ' || *line == L'\t')
        ++line;
    BOOL quoted = *line == L'"';
    if (quoted)
        ++line;
    unsigned n = 0;
    while (*line && (quoted ? *line != L'"' : (*line != L' ' && *line != L'\t'))) {
        if (n >= MAX_PATH - 1)
            return NULL;
        word[n++] = *line++;
    }
    word[n] = 0;
    if (quoted && *line == L'"')
        ++line;
    return n ? line : NULL;
}

void inject_entry(void) {
    static wchar_t self[MAX_PATH], target[MAX_PATH], early[4096], late[4096], log[4096];
    static char marker[256];
    wchar_t *line = GetCommandLineW();
    wchar_t *target_command;
    if (!(line = skip_argument(line, self)) || !(target_command = line) ||
        !skip_argument(line, target)) {
        say("usage: fol-inject.exe GAME.exe [ARGS...]\n");
        ExitProcess(64);
    }
    while (*target_command == L' ' || *target_command == L'\t')
        ++target_command;
    if (GetEnvironmentVariableW(L"FOL_INJECT_STATUS", log, 4096) - 1 < 4095)
        status = CreateFileW(log, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD early_size = GetEnvironmentVariableW(L"FOL_INJECT_EARLY", early, 4096);
    DWORD late_size = GetEnvironmentVariableW(L"FOL_INJECT_LATE", late, 4096);
    DWORD log_size = GetEnvironmentVariableW(L"FOL_INJECT_LOG", log, 4096);
    DWORD marker_size = GetEnvironmentVariableA("FOL_INJECT_MARKER", marker, sizeof(marker));
    if (early_size >= 4096 || late_size >= 4096 || log_size >= 4096 ||
        marker_size >= sizeof(marker) || (late_size && (!log_size || !marker_size))) {
        say("fol-inject: invalid environment\n");
        ExitProcess(64);
    }
    wchar_t delay_text[16];
    DWORD delay = 5000, delay_size = GetEnvironmentVariableW(L"FOL_INJECT_DELAY", delay_text, 16);
    if (delay_size && delay_size < 16) {
        delay = 0;
        for (DWORD i = 0; i < delay_size && delay_text[i] >= L'0' && delay_text[i] <= L'9'; ++i)
            delay = delay * 10 + (DWORD)(delay_text[i] - L'0');
    }
    /* Kill the game if this launcher is terminated. */
    HANDLE job = CreateJobObjectW(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job)
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION child = {0};
    startup.cb = sizeof(startup);
    if (!CreateProcessW(target, target_command, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL,
                        &startup, &child)) {
        say("fol-inject: CreateProcess failed ");
        say_number(GetLastError());
        say("\n");
        ExitProcess(66);
    }
    if (job)
        AssignProcessToJobObject(job, child.hProcess);
    ResumeThread(child.hThread);
    CloseHandle(child.hThread);
    say("fol-inject: started pid ");
    say_number(child.dwProcessId);
    say("\n");
    if (early_size) {
        if (WaitForSingleObject(child.hProcess, 3000) == WAIT_TIMEOUT && !inject(&child, early))
            say("fol-inject: early module failed; continuing\n");
    }
    if (late_size) {
        if (wait_for_marker(child.hProcess, log, marker)) {
            say("fol-inject: marker seen\n");
            if (WaitForSingleObject(child.hProcess, delay) == WAIT_TIMEOUT && !inject(&child, late))
                say("fol-inject: late module failed\n");
        } else
            say("fol-inject: child exited before marker\n");
    }
    WaitForSingleObject(child.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(child.hProcess, &code);
    say("fol-inject: child exited ");
    say_number(code);
    say("\n");
    ExitProcess(code);
}
