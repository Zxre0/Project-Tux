/* Loaded into the local game server only. The server has no use for its
 * window, but under Proton it still receives focus, input and close requests
 * (WM_CLOSE / SC_CLOSE from any source). Unreal then destroys its viewport and
 * the game-server module crashes on a missing world. This guard keeps every
 * top-level window of the process hidden, disabled and uncloseable. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define MAX_GUARDED 32
static struct {
    HWND window;
    WNDPROC original;
} guarded[MAX_GUARDED];
static LONG guarded_count;

static WNDPROC original_for(HWND window) {
    for (LONG i = 0; i < guarded_count; ++i)
        if (guarded[i].window == window)
            return guarded[i].original;
    return NULL;
}

static LRESULT CALLBACK guard_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_CLOSE || message == WM_QUERYENDSESSION)
        return 0;
    if (message == WM_SYSCOMMAND && (wparam & 0xFFF0) == SC_CLOSE)
        return 0;
    if (message == WM_MOUSEACTIVATE)
        return MA_NOACTIVATEANDEAT;
    WNDPROC original = original_for(window);
    return original ? CallWindowProcW(original, window, message, wparam, lparam)
                    : DefWindowProcW(window, message, wparam, lparam);
}

static BOOL CALLBACK guard_window(HWND window, LPARAM unused) {
    (void)unused;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid != GetCurrentProcessId() || original_for(window) || guarded_count >= MAX_GUARDED)
        return TRUE;
    WNDPROC original = (WNDPROC)GetWindowLongPtrW(window, GWLP_WNDPROC);
    if (!original)
        return TRUE;
    guarded[guarded_count].window = window;
    guarded[guarded_count].original = original;
    ++guarded_count;
    SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)guard_proc);
    SetWindowLongPtrW(window, GWL_EXSTYLE,
                      GetWindowLongPtrW(window, GWL_EXSTYLE) | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
    EnableWindow(window, FALSE);
    ShowWindow(window, SW_HIDE);
    return TRUE;
}

static DWORD WINAPI guard_worker(void *unused) {
    (void)unused;
    for (;;) { /* The engine creates its window after start-up; keep watching. */
        EnumWindows(guard_window, 0);
        Sleep(500);
    }
}

BOOL WINAPI guard_entry(HINSTANCE module, DWORD reason, void *reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(NULL, 0, guard_worker, NULL, 0, NULL);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
