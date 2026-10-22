/* Loaded into the local game server only. The server has no use for its
 * window, but under Proton it still receives focus, input and close requests
 * (WM_CLOSE / SC_CLOSE from any source). Unreal then destroys its viewport and
 * the game-server module crashes on a missing world. This guard keeps every
 * top-level window of the process hidden, disabled and uncloseable.
 *
 * Once the server listens on its game port, the windows are also minimized:
 * Slate keeps painting a window that is only hidden, and on 3.1 (UE 4.20,
 * -nullrhi) that window's vertex buffers are never consumed, so they grew
 * until the server ran out of memory after a few hours. Slate skips minimized
 * windows. Minimizing earlier, while the engine starts, stops it from starting. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iphlpapi.h>

#define MAX_GUARDED 32
#define GAME_PORT 7777 // The supervisors host one match on UDP 7777.
static struct {
    HWND window;
    WNDPROC original;
} guarded[MAX_GUARDED];
static LONG guarded_count;
static BOOL listening;

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

static void minimize(HWND window) {
    if (IsIconic(window))
        return;
    ShowWindow(window, SW_SHOWMINNOACTIVE);
    ShowWindow(window, SW_HIDE);
}

static BOOL CALLBACK guard_window(HWND window, LPARAM unused) {
    (void)unused;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid != GetCurrentProcessId())
        return TRUE;
    if (original_for(window)) {
        if (listening)
            minimize(window);
        return TRUE;
    }
    if (guarded_count >= MAX_GUARDED)
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

/* TRUE once something on this machine (in practice this game server) has bound
 * the game port, i.e. the match map is loaded. */
static BOOL game_port_bound(void) {
    static BYTE table[64 * 1024];
    ULONG size = sizeof(table);
    if (GetUdpTable((MIB_UDPTABLE *)table, &size, FALSE) != NO_ERROR)
        return FALSE;
    const MIB_UDPTABLE *udp = (const MIB_UDPTABLE *)table;
    for (DWORD i = 0; i < udp->dwNumEntries; ++i)
        if (((udp->table[i].dwLocalPort & 0xFF) << 8 | (udp->table[i].dwLocalPort >> 8 & 0xFF)) ==
            GAME_PORT)
            return TRUE;
    return FALSE;
}

static DWORD WINAPI guard_worker(void *unused) {
    (void)unused;
    for (;;) { /* The engine creates its window after start-up; keep watching. */
        if (!listening)
            listening = game_port_bound();
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
