/* Load one explicitly supplied project DLL into the child created by this helper.
 * No arbitrary PID, debugger attachment, game-code patch or security override. */
#include "module_window.h"
static BOOL inject_owned_module(PROCESS_INFORMATION *child, const wchar_t *path,
                                const wchar_t *name, const char *loaded_event) {
    DWORD count = (DWORD)length(path);
    if (!count || count >= 4096 || image_base(child->dwProcessId, name)) {
        event("module_name_or_existing_load_rejected", 1);
        return FALSE;
    }
    FARPROC loader = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HMODULE containing;
    wchar_t containing_path[MAX_PATH];
    if (!loader ||
        !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)loader, &containing) ||
        !GetModuleFileNameW(containing, containing_path, MAX_PATH)) {
        event("module_loader_resolution_failed", GetLastError());
        return FALSE;
    }
    /* LoadLibrary may be forwarded to KernelBase. Resolve its actual containing
     * module in the child instead of assuming equal process image addresses. */
    const wchar_t *containing_name = containing_path;
    for (DWORD i = 0; containing_path[i]; ++i)
        if (containing_path[i] == L'\\' || containing_path[i] == L'/')
            containing_name = containing_path + i + 1;
    ULONG_PTR remote_module = image_base(child->dwProcessId, containing_name);
    if (!remote_module)
        return FALSE;
    ULONG_PTR remote_loader = remote_module + (ULONG_PTR)loader - (ULONG_PTR)containing;
    BYTE remote_code[16];
    if (!read_exact(child->hProcess, remote_loader, remote_code, sizeof(remote_code)))
        return FALSE;
    for (unsigned i = 0; i < sizeof(remote_code); ++i)
        if (remote_code[i] != ((const BYTE *)loader)[i]) {
            event("module_loader_runtime_mismatch", 1);
            return FALSE;
        }
    SIZE_T bytes = (count + 1) * sizeof(wchar_t), written = 0;
    void *remote_path =
        VirtualAllocEx(child->hProcess, NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_path) {
        event("module_path_allocation_failed", GetLastError());
        return FALSE;
    }
    if (!WriteProcessMemory(child->hProcess, remote_path, path, bytes, &written) ||
        written != bytes) {
        event("module_path_copy_failed", GetLastError());
        VirtualFreeEx(child->hProcess, remote_path, 0, MEM_RELEASE);
        return FALSE;
    }
    HANDLE thread = CreateRemoteThread(child->hProcess, NULL, 0,
                                       (LPTHREAD_START_ROUTINE)remote_loader, remote_path, 0, NULL);
    if (!thread) {
        event("module_thread_failed", GetLastError());
        VirtualFreeEx(child->hProcess, remote_path, 0, MEM_RELEASE);
        return FALSE;
    }
    DWORD waited = WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    if (waited != WAIT_OBJECT_0) {
        event("module_load_timeout", waited);
        // The loader may still use this allocation. Leave it until the owned
        // child is terminated; do not free memory under a running thread.
        return FALSE;
    }
    if (!VirtualFreeEx(child->hProcess, remote_path, 0, MEM_RELEASE)) {
        event("module_path_release_failed", GetLastError());
        return FALSE;
    }
    BOOL loaded = image_base(child->dwProcessId, name) != 0;
    event(loaded_event, loaded);
    return loaded;
}
static BOOL load_owned_module(PROCESS_INFORMATION *child) {
    static wchar_t path[4096], sdk_path[4096];
    DWORD count = GetEnvironmentVariableW(L"FOL_HTTP_MODULE", path, 4096);
    if (!count || count >= 4096)
        return FALSE;
    const wchar_t *name = path;
    for (DWORD i = 0; i < count; ++i)
        if (path[i] == L'\\' || path[i] == L'/')
            name = path + i + 1;
    BOOL observer =
        equal(name, L"fol-http-request-observer.dll") || equal(name, L"fol-http-local-router.dll");
    if (!observer && !equal(name, L"fol-http-adapter.dll")) {
        event("module_name_or_existing_load_rejected", 1);
        return FALSE;
    }
    BOOL loaded = inject_owned_module(child, path, name, "module_loaded");
    if (!loaded)
        return FALSE;
    count = GetEnvironmentVariableW(L"FOL_SOLO_SDK_MODULE", sdk_path, 4096);
    if (count) {
        if (count >= 4096 || !equal(name, L"fol-http-local-router.dll"))
            return FALSE;
        const wchar_t *sdk_name = sdk_path;
        for (DWORD i = 0; i < count; ++i)
            if (sdk_path[i] == L'\\' || sdk_path[i] == L'/')
                sdk_name = sdk_path + i + 1;
        if (!equal(sdk_name, L"fol-solo-sdk-probe.dll") ||
            !inject_owned_module(child, sdk_path, sdk_name, "sdk_probe_loaded"))
            return FALSE;
    }
    DWORD window = module_window_ms();
    if (!window) {
        event("module_window_rejected", 1);
        return FALSE;
    }
    DWORD wait = WaitForSingleObject(child->hProcess, observer ? window + 5000 : 20000);
    event("module_control_child_live", wait == WAIT_TIMEOUT);
    if (wait != WAIT_TIMEOUT) {
        DWORD exit_code;
        if (GetExitCodeProcess(child->hProcess, &exit_code))
            event("module_child_exit", exit_code);
    }
    return wait == WAIT_TIMEOUT;
}
