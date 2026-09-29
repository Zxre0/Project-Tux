/* Optional diagnostic variant only. Observe an owned child's HTTP function with
 * the Windows debugging API; no code patch, injection, URL rewrite or auth change.
 * Include after http_probe.c's read/output helpers. */
static BOOL starts_with(const wchar_t *value, const wchar_t *prefix) {
    while (*prefix)
        if (*value++ != *prefix++)
            return FALSE;
    return TRUE;
}
static BOOL parse_layout(DWORD *fields) {
    wchar_t text[128];
    DWORD n = GetEnvironmentVariableW(L"FOL_HTTP_OBSERVER_LAYOUT", text, 128);
    if (!n || n >= 128)
        return FALSE;
    const wchar_t *p = text;
    for (unsigned i = 0; i < 4; ++i) {
        DWORD value = 0;
        if (*p < L'0' || *p > L'9')
            return FALSE;
        while (*p >= L'0' && *p <= L'9') {
            if (value > 26843545)
                return FALSE;
            value = value * 10 + (DWORD)(*p++ - L'0');
        }
        fields[i] = value;
        if (i < 3 ? *p++ != L',' : *p != 0)
            return FALSE;
    }
    return fields[2] < 4096 && fields[3] < 64;
}
static BOOL install_breakpoint(HANDLE thread, ULONG_PTR address, DWORD64 *dr7, DWORD64 *dr6) {
    CONTEXT context = {0};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(thread, &context))
        return FALSE;
    /* Never overwrite another debugger's active slot or settings. */
    if ((context.Dr7 & 0xff) || context.Dr0)
        return FALSE;
    *dr7 = context.Dr7;
    *dr6 = context.Dr6;
    context.Dr0 = address;
    context.Dr7 = (context.Dr7 & ~0xf0000ULL) | 1;
    return SetThreadContext(thread, &context);
}
static BOOL inspect_request(HANDLE process, ULONG_PTR object, ULONG_PTR vtable, DWORD offset) {
    ULONG_PTR actual_table;
    struct {
        ULONG_PTR data;
        LONG count, capacity;
    } value;
    static wchar_t url[4096];
    if (!object || !read_exact(process, object, &actual_table, sizeof(actual_table)) ||
        actual_table != vtable || object + offset < object ||
        !read_exact(process, object + offset, &value, sizeof(value)) || value.count < 1 ||
        value.count > 4096 || value.capacity < value.count || value.capacity > 1024 * 1024 ||
        !value.data ||
        !read_exact(process, value.data, url, (SIZE_T)value.count * sizeof(wchar_t)) ||
        url[value.count - 1] != 0) {
        event("request_shape_rejected", 1);
        return FALSE;
    }
    const wchar_t *host;
    if (starts_with(url, L"https://"))
        host = url + 8;
    else if (starts_with(url, L"http://"))
        host = url + 7;
    else {
        event("request_scheme_rejected", 1);
        return FALSE;
    }
    const wchar_t *path = host;
    while (*path && *path != L'/' && *path != L'?' && *path != L'#')
        ++path;
    const char *origin = "other";
    if (starts_with(host, L"127.0.0.1:") || starts_with(host, L"localhost:"))
        origin = "loopback";
    else {
        const wchar_t suffix[] = L".epicgames.com";
        SIZE_T host_length = (SIZE_T)(path - host);
        SIZE_T suffix_length = sizeof(suffix) / sizeof(wchar_t) - 1;
        if (host_length >= suffix_length) {
            BOOL match = TRUE;
            for (SIZE_T i = 0; i < suffix_length; ++i)
                match &= host[host_length - suffix_length + i] == suffix[i];
            if (match)
                origin = "epic";
        }
    }
    const char *route = "other";
    if (starts_with(path, L"/account/api/oauth/token"))
        route = "oauth-token";
    else if (starts_with(path, L"/account/api/public/"))
        route = "account-public";
    else if (starts_with(path, L"/fortnite/api/game/v2/profile/"))
        route = "fortnite-profile";
    else if (starts_with(path, L"/fortnite/api/version"))
        route = "fortnite-version";
    else if (starts_with(path, L"/lightswitch/"))
        route = "lightswitch";
    else if (starts_with(path, L"/launcher/"))
        route = "launcher";
    else if (starts_with(path, L"/content/"))
        route = "content";
    /* Deliberately never emit the URL, host, query, body, headers or object
     * contents. Only these fixed non-sensitive categories leave this function. */
    output("FOL_HTTP_PROBE {\"event\":\"request_observed\",\"origin\":\"");
    output(origin);
    output("\",\"route\":\"");
    output(route);
    output("\"}\n");
    memset(url, 0, sizeof(url));
    return TRUE;
}
static BOOL observe_requests(PROCESS_INFORMATION *child, ULONG_PTR base, DWORD function,
                             DWORD reference, BOOL fixture) {
    DWORD layout[4];
    wchar_t mode[16];
    DWORD mode_size = GetEnvironmentVariableW(L"FOL_HTTP_OBSERVER_MODE", mode, 16);
    BOOL attach_only = mode_size && mode_size < 16 && equal(mode, L"attach-only");
    BOOL hold_only = mode_size && mode_size < 16 && equal(mode, L"hold-only");
    if (mode_size && !attach_only && !hold_only)
        return FALSE;
    if (fixture) {
        layout[0] = function;
        layout[1] = reference;
        layout[2] = 104; // Synthetic fixture's explicitly asserted layout.
        layout[3] = 0;
    } else if (!parse_layout(layout)) {
        event("observer_layout_rejected", 1);
        return FALSE;
    }
    ULONG_PTR target;
    if (layout[0] != function || layout[1] + layout[3] * 8 != reference ||
        !read_exact(child->hProcess, base + reference, &target, sizeof(target)) ||
        target != base + function) {
        event("observer_layout_rejected", 2);
        return FALSE;
    }
    wchar_t delay_text[8];
    DWORD delay_length = GetEnvironmentVariableW(L"FOL_HTTP_OBSERVER_DELAY_MS", delay_text, 8);
    if (delay_length) {
        DWORD delay = 0;
        if (delay_length >= 8)
            return FALSE;
        for (DWORD i = 0; i < delay_length; ++i) {
            if (delay_text[i] < L'0' || delay_text[i] > L'9')
                return FALSE;
            delay = delay * 10 + (DWORD)(delay_text[i] - L'0');
        }
        if (delay > 30000)
            return FALSE;
        event("observer_delay_ms", delay);
        if (WaitForSingleObject(child->hProcess, delay) != WAIT_TIMEOUT) {
            event("observer_child_exited_before_attach", 1);
            return FALSE;
        }
    }
    if (hold_only) {
        event("observer_control", 1);
        DWORD wait = WaitForSingleObject(child->hProcess, 20000);
        event("observer_control_child_live", wait == WAIT_TIMEOUT);
        return FALSE; // A control run is never a successful request observation.
    }
    if (attach_only)
        event("observer_control", 2);
    if (!DebugActiveProcess(child->dwProcessId)) {
        event("debug_attach_failed", GetLastError());
        return FALSE;
    }
    event("debug_attached", child->dwProcessId);
    static DWORD tids[1024];
    static DWORD64 original_dr7[1024], original_dr6[1024];
    DWORD count = 0, observed = 0;
    BOOL okay = TRUE, initial_break = TRUE, exited = FALSE;
    ULONGLONG deadline = GetTickCount64() + 20000;
    while (okay && !exited && observed < 8 && GetTickCount64() < deadline) {
        DEBUG_EVENT debug = {0};
        if (!WaitForDebugEvent(&debug, 250)) {
            if (GetLastError() == ERROR_SEM_TIMEOUT)
                continue;
            event("debug_wait_failed", GetLastError());
            okay = FALSE;
            break;
        }
        DWORD disposition = DBG_CONTINUE;
        if (debug.dwProcessId != child->dwProcessId) {
            event("debug_unexpected_process", debug.dwProcessId);
            ContinueDebugEvent(debug.dwProcessId, debug.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
            okay = FALSE;
            break;
        }
        HANDLE thread = NULL;
        if (debug.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
            thread = debug.u.CreateProcessInfo.hThread;
            if (debug.u.CreateProcessInfo.hFile)
                CloseHandle(debug.u.CreateProcessInfo.hFile);
            CloseHandle(debug.u.CreateProcessInfo.hProcess);
        } else if (debug.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT) {
            thread = debug.u.CreateThread.hThread;
        } else if (debug.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) {
            if (debug.u.LoadDll.hFile)
                CloseHandle(debug.u.LoadDll.hFile);
        } else if (debug.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
            exited = TRUE;
            event("debug_child_exited", debug.u.ExitProcess.dwExitCode);
        } else if (debug.dwDebugEventCode == EXIT_THREAD_DEBUG_EVENT) {
            for (DWORD i = 0; i < count; ++i)
                if (tids[i] == debug.dwThreadId)
                    tids[i] = 0;
        } else if (debug.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            DWORD exception = debug.u.Exception.ExceptionRecord.ExceptionCode;
            disposition = DBG_EXCEPTION_NOT_HANDLED;
            if (exception == EXCEPTION_BREAKPOINT && initial_break) {
                initial_break = FALSE;
                disposition = DBG_CONTINUE;
            } else if (exception == EXCEPTION_SINGLE_STEP &&
                       (ULONG_PTR)debug.u.Exception.ExceptionRecord.ExceptionAddress == target) {
                HANDLE current =
                    OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, debug.dwThreadId);
                CONTEXT context = {0};
                context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
                if (!current || !GetThreadContext(current, &context) || !(context.Dr6 & 1)) {
                    okay = FALSE;
                } else {
                    okay =
                        inspect_request(child->hProcess, context.Rcx, base + layout[1], layout[2]);
                    if (okay)
                        ++observed;
                    context.EFlags |= 0x10000; // Resume the original instruction exactly once.
                    context.Dr6 = 0;
                    if (!SetThreadContext(current, &context))
                        okay = FALSE;
                    disposition = DBG_CONTINUE;
                }
                if (current)
                    CloseHandle(current);
            }
        }
        if (thread) {
            if (attach_only) {
                CloseHandle(thread);
                thread = NULL;
            } else if (count == 1024 || !install_breakpoint(thread, target, &original_dr7[count],
                                                            &original_dr6[count])) {
                event("debug_thread_setup_failed", GetLastError());
                okay = FALSE;
            } else {
                tids[count++] = debug.dwThreadId;
            }
            if (thread)
                CloseHandle(thread);
        }
        if (!ContinueDebugEvent(debug.dwProcessId, debug.dwThreadId, disposition)) {
            event("debug_continue_failed", GetLastError());
            okay = FALSE;
        }
    }
    if (!exited) {
        for (DWORD i = 0; i < count; ++i) {
            if (!tids[i])
                continue;
            HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT |
                                           THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
                                       FALSE, tids[i]);
            if (!thread) {
                if (GetLastError() != ERROR_INVALID_PARAMETER) {
                    event("debug_cleanup_open_failed", GetLastError());
                    okay = FALSE;
                }
                continue;
            }
            if (GetProcessIdOfThread(thread) != child->dwProcessId) {
                event("debug_thread_owner_mismatch", tids[i]);
                CloseHandle(thread);
                okay = FALSE;
                continue;
            }
            DWORD exit_code;
            if (GetExitCodeThread(thread, &exit_code) && exit_code != STILL_ACTIVE) {
                CloseHandle(thread);
                continue;
            }
            if (SuspendThread(thread) != (DWORD)-1) {
                CONTEXT context = {0};
                context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(thread, &context) && context.Dr0 == target) {
                    context.Dr0 = 0;
                    context.Dr7 = original_dr7[i];
                    context.Dr6 = original_dr6[i];
                    if (!SetThreadContext(thread, &context))
                        okay = FALSE;
                } else {
                    okay = FALSE;
                }
                if (ResumeThread(thread) == (DWORD)-1)
                    okay = FALSE;
            } else {
                event("debug_cleanup_suspend_failed", GetLastError());
                okay = FALSE;
            }
            CloseHandle(thread);
        }
        if (!DebugActiveProcessStop(child->dwProcessId)) {
            event("debug_detach_failed", GetLastError());
            okay = FALSE;
        }
    }
    event("request_observation_completed", okay && observed > 0);
    return okay && observed > 0;
}
