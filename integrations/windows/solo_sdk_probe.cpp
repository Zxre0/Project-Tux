// SPDX-License-Identifier: GPL-3.0-only
// Original compatibility probe using the pinned Erbium SDK headers only.
// No Erbium server entry point, hooks, authentication changes or code patches.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Includes.h"

static HANDLE report = INVALID_HANDLE_VALUE;
static char session[128];
static DWORD game_thread;
static HWND game_window;
static WNDPROC previous_window_proc;
static UINT travel_message;
static ULONG_PTR travel_cookie;
static wchar_t travel_world[129];
static bool travel_issued;

static bool record(const char* event, unsigned value) {
    char line[512];
    const int length = std::snprintf(line, sizeof(line),
        "{\"event\":\"%s\",\"session_id\":\"%s\",\"process_id\":%lu,\"value\":%u}\n",
        event, session, GetCurrentProcessId(), value);
    DWORD written = 0;
    return length > 0 && length < sizeof(line) &&
        WriteFile(report, line, length, &written, nullptr) && written == length &&
        FlushFileBuffers(report);
}

static bool inspect_sdk() {
    if (!record("sdk-init-started", 1)) return false;
    SDK::Init();
    if (!record("sdk-init-completed", 1)) return false;
    const auto version = static_cast<unsigned>(SDK::VersionInfo.FortniteVersion * 100 + 0.5);
    const auto engine = static_cast<unsigned>(SDK::VersionInfo.EngineVersion * 100 + 0.5);
    if (!record("fortnite-version-times100", version) ||
        !record("engine-version-times100", engine)) return false;
    if (version != 1250 || engine != 425) return false;
    if (!SDK::Offsets::GObjectsChunked || !SDK::Offsets::FNameConstructor ||
        !SDK::Offsets::AppendString) return false;
    const auto count = SDK::TUObjectArray::Num();
    if (!record("object-count", count) || count < 1 || count > 2000000) return false;
    const auto mode = SDK::FindClass("FortGameModeAthena");
    const auto driver = SDK::FindClass("IpNetDriver");
    if (!record("game-mode-class-found", mode != nullptr) ||
        !record("net-driver-class-found", driver != nullptr)) return false;
    // UObject::GetFunction starts at the object's Class. A UClass object's Class
    // is the metaclass, not FortGameModeAthena; use its already-existing CDO.
    const auto defaultMode = mode ? mode->GetDefaultObj() : nullptr;
    const auto ready = defaultMode ? defaultMode->GetFunction("ReadyToStartMatch") : nullptr;
    if (!record("ready-to-start-function-found", ready != nullptr) ||
        !record("process-event-vtable-slot", SDK::Offsets::ProcessEventVft)) return false;
    const auto gameEngine = SDK::UEngine::GetEngine();
    const auto viewport = gameEngine ? gameEngine->GameViewport : nullptr;
    const auto world = viewport ? viewport->World : nullptr;
    if (!record("world-found", world != nullptr)) return false;
    // Reflection observations do not establish listening, admission or gameplay.
    return mode && driver && ready && world && SDK::Offsets::ProcessEventVft > 0;
}

static bool guarded_cpp_probe() {
    try { return inspect_sdk(); }
    catch (const std::exception&) { record("cpp-exception", 1); return false; }
    catch (...) { record("unknown-cpp-exception", 1); return false; }
}

static bool issue_world_travel() {
    try {
        const auto engine = SDK::UEngine::GetEngine();
        const auto viewport = engine ? engine->GameViewport : nullptr;
        const auto world = viewport ? viewport->World : nullptr;
        const auto library = SDK::UKismetSystemLibrary::GetDefaultObj();
        if (!world || !library || !library->GetFunction("ExecuteConsoleCommand")) return false;
        const std::wstring command = std::wstring(L"open ") + travel_world;
        // Ordinary engine world-travel request only. No listener/admission,
        // exit suppression, anti-cheat, authentication or encryption changes.
        SDK::UKismetSystemLibrary::ExecuteConsoleCommand(world, SDK::FString(command.c_str()), nullptr);
        return record("world-travel-request-issued", 1);
    } catch (const std::exception&) { record("cpp-exception", 1); }
      catch (...) { record("unknown-cpp-exception", 1); }
    return false;
}

static LRESULT CALLBACK travel_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    const auto previous = previous_window_proc;
    if (window != game_window || message != travel_message || wparam != travel_cookie || lparam != 0)
        return CallWindowProcW(previous, window, message, wparam, lparam);
    // Restore before invoking any engine API. This is a one-shot Win32 window
    // subclass, not a game-code/ProcessEvent hook. Never invoke from the worker.
    if (GetCurrentThreadId() != game_thread ||
        GetWindowLongPtrW(window, GWLP_WNDPROC) != reinterpret_cast<LONG_PTR>(travel_window_proc))
        return 0;
    SetLastError(0);
    if (!SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous)) && GetLastError())
        return 0;
    if (!record("window-thread-verified", 1)) return 0;
    __try { travel_issued = issue_world_travel(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { record("structured-exception", GetExceptionCode()); }
    return travel_issued ? 1 : 0;
}

static BOOL CALLBACK find_game_window(HWND window, LPARAM) {
    DWORD pid = 0;
    const DWORD thread = GetWindowThreadProcessId(window, &pid);
    if (pid == GetCurrentProcessId() && thread == game_thread && IsWindowVisible(window) &&
        !GetWindow(window, GW_OWNER)) {
        if (game_window) { game_window = nullptr; return FALSE; }
        game_window = window;
    }
    return TRUE;
}

static bool request_game_thread_travel() {
    const DWORD size = GetEnvironmentVariableW(L"FOL_SOLO_SDK_TRAVEL", travel_world, 129);
    if (!size) return true; // Baseline SDK-only probe remains unchanged.
    if (size >= 129) return false;
    for (DWORD i = 0; i < size; ++i) {
        const auto c = travel_world[i];
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (c >= L'0' && c <= L'9') || c == L'_')) return false;
    }
    wchar_t context_path[4096];
    const auto pathSize = GetEnvironmentVariableW(L"FOL_SOLO_SDK_CONTEXT", context_path, 4096);
    if (!pathSize || pathSize >= 4096) return false;
    HANDLE context = CreateFileW(context_path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (context == INVALID_HANDLE_VALUE) return false;
    DWORD identity[3] = {}, bytes = 0;
    const bool read = GetFileSize(context, nullptr) == sizeof(identity) &&
        ReadFile(context, identity, sizeof(identity), &bytes, nullptr) && bytes == sizeof(identity);
    CloseHandle(context);
    if (!read || identity[0] != 0x314c4f46 || identity[1] != GetCurrentProcessId() || !identity[2]) return false;
    game_thread = identity[2];
    EnumWindows(find_game_window, 0);
    if (!game_window) return false;
    wchar_t messageName[160];
    std::swprintf(messageName, 160, L"FOL-World-Travel-%hs", session);
    travel_message = RegisterWindowMessageW(messageName);
    travel_cookie = static_cast<ULONG_PTR>(GetTickCount64()) ^ GetCurrentProcessId() ^ game_thread;
    if (!travel_message || !travel_cookie) return false;
    SetLastError(0);
    previous_window_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        game_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(travel_window_proc)));
    if (!previous_window_proc) return false;
    DWORD_PTR response = 0;
    const bool delivered = SendMessageTimeoutW(game_window, travel_message, travel_cookie, 0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 10000, &response) != 0;
    // A timed-out callback may still be queued. Remove only our subclass; never
    // overwrite a later subclass installed by another game component.
    if (GetWindowLongPtrW(game_window, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(travel_window_proc))
        SetWindowLongPtrW(game_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous_window_proc));
    return delivered && response == 1 && travel_issued;
}

static DWORD WINAPI worker(void*) {
    wchar_t path[4096], identity[128], executable[4096];
    const auto size = GetEnvironmentVariableW(L"FOL_SOLO_SDK_REPORT", path, 4096);
    const auto idSize = GetEnvironmentVariableW(L"FOL_SESSION_ID", identity, 128);
    if (!size || size >= 4096 || !idSize || idSize >= 128) return 64;
    for (DWORD i = 0; i < idSize; ++i) {
        const auto c = identity[i];
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (c >= L'0' && c <= L'9') || c == L'-' || c == L'_')) return 64;
        session[i] = static_cast<char>(c);
    }
    const DWORD executableSize = GetModuleFileNameW(nullptr, executable, 4096);
    if (!executableSize || executableSize >= 4096) return 65;
    const auto name = wcsrchr(executable, L'\\');
    if (!name || wcscmp(name + 1, L"FortniteClient-Win64-Shipping.exe") != 0) return 65;
    report = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
    if (report == INVALID_HANDLE_VALUE) return 68;
    bool successful = false;
    if (record("started", 1)) {
        // Leave startup and local login unchanged. Inspect after ordinary engine
        // initialization; never suppress a game exit if prerequisites are absent.
        Sleep(30000);
        __try {
            successful = guarded_cpp_probe();
            if (successful) successful = request_game_thread_travel();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { record("structured-exception", GetExceptionCode()); }
    }
    const bool complete = record("completed", successful ? 1 : 0);
    CloseHandle(report);
    report = INVALID_HANDLE_VALUE;
    return complete && successful ? 0 : 67;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (!thread) return FALSE;
        CloseHandle(thread);
    }
    return TRUE;
}
