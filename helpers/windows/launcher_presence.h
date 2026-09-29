/* Explicit launcher-presence experiment. The supplied bootstrap never runs its
 * entry point: it remains suspended and belongs to our kill-on-close job.
 * This changes observable process context, not authorization or EAC state. */
static BOOL start_launcher_presence(HANDLE job, HANDLE *process) {
    /* Single-use helper; static buffers avoid a CRT stack-probe dependency. */
    static wchar_t path[4096], command[4098];
    *process = NULL;
    SetLastError(ERROR_SUCCESS);
    DWORD size = GetEnvironmentVariableW(L"FOL_LAUNCHER_PRESENCE_PATH", path, 4096);
    if (!size && GetLastError() == ERROR_ENVVAR_NOT_FOUND)
        return TRUE;
    if (!size || size >= 4096) {
        event("launcher_presence_path_rejected", 1);
        return FALSE;
    }
    const wchar_t *name = path;
    for (DWORD i = 0; i < size; ++i) {
        if (path[i] == L'"') {
            event("launcher_presence_path_rejected", 1);
            return FALSE;
        }
        if (path[i] == L'\\' || path[i] == L'/')
            name = path + i + 1;
        command[i + 1] = path[i];
    }
    if (!equal(name, L"FortniteLauncher.exe")) {
        event("launcher_presence_path_rejected", 1);
        return FALSE;
    }
    command[0] = L'"';
    command[size + 1] = L'"';
    command[size + 2] = 0;
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION launcher = {0};
    startup.cb = sizeof(startup);
    if (!CreateProcessW(path, command, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup,
                        &launcher)) {
        event("launcher_presence_start_failed", GetLastError());
        return FALSE;
    }
    if (!AssignProcessToJobObject(job, launcher.hProcess)) {
        event("launcher_presence_job_failed", GetLastError());
        TerminateProcess(launcher.hProcess, 66);
        WaitForSingleObject(launcher.hProcess, 5000);
        CloseHandle(launcher.hThread);
        CloseHandle(launcher.hProcess);
        return FALSE;
    }
    CloseHandle(launcher.hThread); /* Deliberately never ResumeThread. */
    *process = launcher.hProcess;
    event("launcher_presence_suspended", launcher.dwProcessId);
    return TRUE;
}
