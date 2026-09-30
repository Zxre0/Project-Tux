/* Explicit UI diagnostic, not a game/module hook. Run only in the owned prefix
 * with the game PID from this session's helper receipt. Sending input does
 * not prove the game accepted input; verify a rendered UI transition. Only the
 * observed 1920x1080 layout and explicitly named non-matchmaking UI actions are
 * supported. Session ownership must be verified by the native caller. */
#include <windows.h>

static DWORD game_pid;
static HWND game_window;
static void record(const char *key, DWORD value) {
    char digits[11];
    DWORD count = 0, written;
    do {
        digits[count++] = '0' + value % 10;
        value /= 10;
    } while (value);
    DWORD length = 0;
    while (key[length])
        ++length;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!WriteFile(output, key, length, &written, NULL) || written != length)
        ExitProcess(68);
    while (count)
        if (!WriteFile(output, digits + --count, 1, &written, NULL) || written != 1)
            ExitProcess(68);
    if (!WriteFile(output, "\n", 1, &written, NULL) || written != 1)
        ExitProcess(68);
}
static BOOL equal(const wchar_t *a, const wchar_t *b) {
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}
static BOOL CALLBACK find_window(HWND window, LPARAM unused) {
    (void)unused;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid == game_pid && IsWindowVisible(window) && !GetWindow(window, GW_OWNER)) {
        if (game_window) {
            game_window = NULL;
            return FALSE;
        }
        game_window = window;
    }
    return TRUE;
}
void probe_entry(void) {
    static wchar_t pid_text[16], session[128], path[4096], action[32];
    DWORD n = GetEnvironmentVariableW(L"FOL_GAME_WINDOWS_PID", pid_text, 16);
    DWORD s = GetEnvironmentVariableW(L"FOL_SESSION_ID", session, 128);
    if (!n || n > 10 || !s || s >= 128)
        ExitProcess(64);
    for (DWORD i = 0; i < n; ++i) {
        if (pid_text[i] < L'0' || pid_text[i] > L'9' || game_pid > 429496728UL)
            ExitProcess(64);
        game_pid = game_pid * 10 + (pid_text[i] - L'0');
    }
    if (!game_pid)
        ExitProcess(64);
    DWORD action_length = GetEnvironmentVariableW(L"FOL_UI_ACTION", action, 32);
    if (!action_length || action_length >= 32 ||
        (action_length && !equal(action, L"select-br") && !equal(action, L"enter-br") &&
         !equal(action, L"skip-intro") && !equal(action, L"locker") &&
         !equal(action, L"play-tab") && !equal(action, L"escape") &&
         !equal(action, L"outfit-slot") && !equal(action, L"outfit-first") &&
         !equal(action, L"outfit-save") && !equal(action, L"match-play") &&
         !equal(action, L"mode-change")))
        ExitProcess(64);
    BOOL enter_br = equal(action, L"enter-br");
    BOOL skip_intro = equal(action, L"skip-intro");
    BOOL locker = equal(action, L"locker"), play_tab = equal(action, L"play-tab");
    BOOL outfit_slot = equal(action, L"outfit-slot");
    BOOL outfit_first = equal(action, L"outfit-first"), outfit_save = equal(action, L"outfit-save");
    BOOL match_play = equal(action, L"match-play"), mode_change = equal(action, L"mode-change");
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, game_pid);
    DWORD length = 4096;
    if (!process || !QueryFullProcessImageNameW(process, 0, path, &length) ||
        WaitForSingleObject(process, 0) != WAIT_TIMEOUT)
        ExitProcess(65);
    const wchar_t *name = path;
    for (DWORD i = 0; path[i]; ++i)
        if (path[i] == L'\\' || path[i] == L'/')
            name = path + i + 1;
    if (!equal(name, L"FortniteClient-Win64-Shipping.exe"))
        ExitProcess(64);
    EnumWindows(find_window, 0);
    RECT rect;
    if (!game_window || !GetClientRect(game_window, &rect) || rect.right != 1920 ||
        rect.bottom != 1080)
        ExitProcess(66);
    record("client_width=", rect.right);
    record("client_height=", rect.bottom);
    record("foreground_before=", GetForegroundWindow() == game_window);
    SetForegroundWindow(game_window);
    Sleep(200);
    record("foreground_after=", GetForegroundWindow() == game_window);
    if (GetForegroundWindow() != game_window || WaitForSingleObject(process, 0) != WAIT_TIMEOUT)
        ExitProcess(67);
    if (equal(action, L"escape")) {
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = VK_ESCAPE;
        UINT down = SendInput(1, &input, sizeof(input));
        Sleep(150);
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        UINT up = SendInput(1, &input, sizeof(input));
        record("input_down_sent=", down);
        record("input_up_sent=", up);
        CloseHandle(process);
        ExitProcess(down == 1 && up == 1 ? 0 : 67);
    }
    LONG y = match_play             ? 675
             : mode_change          ? 588
             : outfit_first         ? 390
             : outfit_save          ? 955
             : outfit_slot          ? 340
             : (locker || play_tab) ? 48
             : skip_intro           ? rect.bottom * 95 / 100
             : enter_br             ? rect.bottom * 82 / 100
                                    : rect.bottom / 2;
    POINT cursor = {(match_play || mode_change) ? 1700
                    : outfit_first ? 410
                    : outfit_save ? 640
                    : outfit_slot ? 450
                    : locker      ? 906
                    : play_tab    ? 526
                    : skip_intro  ? rect.right * 97 / 100
                                  : rect.right / 2,
                    y};
    if (!ClientToScreen(game_window, &cursor) || !SetCursorPos(cursor.x, cursor.y))
        ExitProcess(67);
    Sleep(200); // Let XWayland/Wine deliver the cursor move before the click.
    POINT actual;
    if (!GetCursorPos(&actual))
        ExitProcess(67);
    record("cursor_x=", actual.x);
    record("cursor_y=", actual.y);
    if (actual.x != cursor.x || actual.y != cursor.y)
        ExitProcess(67);
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    UINT down = SendInput(1, &input, sizeof(input));
    Sleep(150);
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    UINT up = SendInput(1, &input, sizeof(input));
    record("input_down_sent=", down);
    record("input_up_sent=", up);
    CloseHandle(process);
    ExitProcess(down == 1 && up == 1 ? 0 : 67);
}
