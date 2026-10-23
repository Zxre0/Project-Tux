/* 12.50's building behaviour for older builds (3.1), in a player's game only:
 *
 *   turbo building  3.1 repeats a held build every 0.15 s (first repeat after
 *                   the same 0.15 s); 12.50 after TurboBuildFirstInterval 0.05 s,
 *                   then every TurboBuildInterval 0.005 s. While the game's turbo
 *                   timer runs, this calls the game's own turbo-build step at
 *                   12.50's pace as well; the step checks the build tool itself
 *                   and stops the timer when building stops.
 *   edit on release 12.50's "Confirm edit on release": an edit is confirmed as
 *                   soon as the select button comes up, or right after a reset.
 *                   The edit tool's "select released" and "reset" methods (what
 *                   the controller's edit-mode input forwards to) are wrapped to
 *                   call the controller's CompleteBuildingEditInteraction, the
 *                   function the edit key calls, right after them.
 *
 * 3.1 checks its own code, so nothing in it is changed: both go through
 * virtual-table slots, like the router. A per-frame controller method (slot
 * `slot` of the match controller's table, called on the game thread) runs the
 * turbo steps and finds the edit tool's table. Sending builds and edits to the
 * server stays the game's own code.
 *
 *   FOL_TURBO_LAYOUT  tick,table,slot,timer,step
 *                     RVAs of the per-frame method and the match controller's
 *                     table and its slot, the controller's turbo timer handle
 *                     offset and the RVA of the turbo-build step
 *   FOL_EDIT_LAYOUT   tick,table,slot,complete,tool,release_call,reset_call
 *                     the same per-frame method, the RVA of
 *                     CompleteBuildingEditInteraction, the controller's edit-tool
 *                     offset and the tool's two method offsets
 *                     (profiles/fortnite/<version>.toml, from the exact build) */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int _fltused; // The per-frame method takes a float; no CRT to define this.

typedef void (*tick_function)(BYTE *, float);
typedef void (*method)(void *);
static tick_function original_tick;
static ULONG_PTR base, image_size;

/* Turbo building */
static BOOL turbo_on;
static DWORD timer_offset;
static method turbo_step;
static LONGLONG turbo_since, turbo_last, ticks_per_second;

/* Edit on release */
static BOOL edit_on;
static DWORD tool_offset, release_call, reset_call;
static method complete_edit, original_release, original_reset;
static BYTE *edit_controller; // The local match controller, set every frame on the game thread.
static BOOL edit_hooked;

static LONGLONG now(void) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

static void edit_after(void *tool, method original) {
    // Only the local controller's own edit tool; checked first, the method may end the edit.
    BYTE *controller = edit_controller;
    BOOL ours = controller && *(void **)(controller + tool_offset) == tool;
    original(tool);
    if (ours)
        complete_edit(controller);
}
static void edit_release(void *tool) { edit_after(tool, original_release); }
static void edit_reset(void *tool) { edit_after(tool, original_reset); }

static BOOL in_image(ULONG_PTR address) { return address > base && address - base < image_size; }

static BOOL swap_slot(void **slot, void *hook, void *expected) {
    MEMORY_BASIC_INFORMATION page;
    if (!VirtualQuery((void *)slot, &page, sizeof(page)) || page.State != MEM_COMMIT)
        return FALSE;
    // Tables are data; keep execute permission where the self-unpacking image has it.
    DWORD writable = page.Protect == PAGE_EXECUTE_WRITECOPY || page.Protect == PAGE_EXECUTE_READWRITE
                         ? PAGE_EXECUTE_READWRITE
                         : PAGE_READWRITE;
    DWORD old, ignored;
    if (!VirtualProtect((void *)slot, sizeof(void *), writable, &old))
        return FALSE;
    BOOL swapped = InterlockedCompareExchangePointer((void *volatile *)slot, hook, expected) == expected;
    VirtualProtect((void *)slot, sizeof(void *), old, &ignored);
    return swapped;
}

/* The edit tool exists while editing; wrap its class's two methods once. */
static void hook_edit_tool(BYTE *controller) {
    BYTE *tool = *(BYTE **)(controller + tool_offset);
    if (!tool)
        return;
    void **table = *(void ***)tool;
    if (!in_image((ULONG_PTR)table))
        return;
    void **release = (void **)((BYTE *)table + release_call), **reset = (void **)((BYTE *)table + reset_call);
    void *release_target = *release, *reset_target = *reset;
    if (!in_image((ULONG_PTR)release_target) || !in_image((ULONG_PTR)reset_target))
        return;
    original_release = (method)release_target;
    original_reset = (method)reset_target;
    edit_hooked = swap_slot(release, (void *)edit_release, release_target) &&
                  swap_slot(reset, (void *)edit_reset, reset_target);
}

static void building_tick(BYTE *controller, float delta) {
    if (edit_on) {
        edit_controller = controller;
        if (!edit_hooked)
            hook_edit_tool(controller);
    }
    if (turbo_on) {
        if (!*(ULONGLONG *)(controller + timer_offset)) {
            turbo_since = 0; // No turbo timer: not building.
        } else if (!turbo_since) {
            turbo_since = turbo_last = now();
        } else {
            LONGLONG t = now();
            // 12.50: TurboBuildFirstInterval 0.05 s, then TurboBuildInterval 0.005 s.
            if (t - turbo_since >= ticks_per_second / 20 && t - turbo_last >= ticks_per_second / 200) {
                turbo_last = t;
                turbo_step(controller);
            }
        }
    }
    original_tick(controller, delta);
}

static BOOL building_read(ULONG_PTR address, void *output, SIZE_T size) {
    SIZE_T read;
    return address && ReadProcessMemory(GetCurrentProcess(), (void *)address, output, size, &read) &&
           read == size;
}

/* Parses `count` comma-separated decimal numbers from an environment variable. */
static BOOL building_layout(const wchar_t *name, DWORD *layout, unsigned count) {
    wchar_t text[160];
    DWORD n = GetEnvironmentVariableW(name, text, 160);
    if (!n || n >= 160)
        return FALSE;
    const wchar_t *p = text;
    for (unsigned i = 0; i < count; ++i) {
        DWORD value = 0;
        if (*p < L'0' || *p > L'9')
            return FALSE;
        while (*p >= L'0' && *p <= L'9') {
            if (value > 26843545)
                return FALSE;
            value = value * 10 + (DWORD)(*p++ - L'0');
        }
        layout[i] = value;
        if (i + 1 < count ? *p++ != L',' : *p != 0)
            return FALSE;
    }
    return TRUE;
}

/* TRUE if [rva, rva + size) lies in one readable section that is (or is not) executable. */
static BOOL building_section(const IMAGE_DOS_HEADER *dos, const IMAGE_NT_HEADERS64 *pe, DWORD rva,
                             SIZE_T size, BOOL executable) {
    if (!pe->FileHeader.NumberOfSections || pe->FileHeader.NumberOfSections > 96)
        return FALSE;
    ULONG_PTR sections = base + dos->e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
                         pe->FileHeader.SizeOfOptionalHeader;
    for (unsigned i = 0; i < pe->FileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER section;
        if (!building_read(sections + i * sizeof(section), &section, sizeof(section)))
            return FALSE;
        if (rva >= section.VirtualAddress &&
            (ULONGLONG)rva + size <= (ULONGLONG)section.VirtualAddress + section.Misc.VirtualSize)
            return (section.Characteristics & IMAGE_SCN_MEM_READ) &&
                   ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) == executable;
    }
    return FALSE;
}

static BOOL building_install(void) {
    base = (ULONG_PTR)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS64 pe;
    if (!building_read(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < 0 || dos.e_lfanew > 1024 * 1024 ||
        !building_read(base + dos.e_lfanew, &pe, sizeof(pe)) || pe.Signature != IMAGE_NT_SIGNATURE ||
        pe.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        pe.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return FALSE;
    image_size = pe.OptionalHeader.SizeOfImage;
    DWORD turbo[5], edit[7], tick[3];
    BOOL want_turbo = building_layout(L"FOL_TURBO_LAYOUT", turbo, 5);
    BOOL want_edit = building_layout(L"FOL_EDIT_LAYOUT", edit, 7);
    if (!want_turbo && !want_edit)
        return FALSE;
    // Both layouts name the same per-frame method.
    for (unsigned i = 0; i < 3; ++i)
        tick[i] = want_turbo ? turbo[i] : edit[i];
    if (want_turbo && want_edit && (turbo[0] != edit[0] || turbo[1] != edit[1] || turbo[2] != edit[2]))
        return FALSE;
    if (tick[1] % 8 || tick[2] >= 4096 || !building_section(&dos, &pe, tick[0], 1, TRUE) ||
        !building_section(&dos, &pe, tick[1] + tick[2] * 8, sizeof(void *), FALSE) ||
        (want_turbo && (turbo[3] >= 0x10000 || !building_section(&dos, &pe, turbo[4], 1, TRUE))) ||
        (want_edit && (edit[4] >= 0x10000 || edit[5] >= 0x10000 || edit[6] >= 0x10000 || edit[5] % 8 ||
                       edit[6] % 8 || !building_section(&dos, &pe, edit[3], 1, TRUE))))
        return FALSE;
    void **slot = (void **)(base + tick[1] + tick[2] * 8);
    ULONG_PTR target;
    if (!building_read((ULONG_PTR)slot, &target, sizeof(target)) || target != base + tick[0])
        return FALSE;
    if (want_turbo) {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        ticks_per_second = frequency.QuadPart;
        timer_offset = turbo[3];
        turbo_step = (method)(base + turbo[4]);
        turbo_on = TRUE;
    }
    if (want_edit) {
        complete_edit = (method)(base + edit[3]);
        tool_offset = edit[4];
        release_call = edit[5];
        reset_call = edit[6];
        edit_on = TRUE;
    }
    original_tick = (tick_function)target;
    return swap_slot(slot, (void *)building_tick, (void *)target);
}

static DWORD WINAPI building_worker(void *unused) {
    (void)unused;
    // The early loader runs while the image may still be unpacking; keep trying for a minute.
    for (unsigned attempt = 0; attempt < 60; ++attempt) {
        if (building_install())
            return 0;
        Sleep(1000);
    }
    return 64;
}

BOOL WINAPI building_entry(HINSTANCE module, DWORD reason, void *reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(NULL, 0, building_worker, NULL, 0, NULL);
        if (!thread)
            return FALSE;
        CloseHandle(thread);
    }
    return TRUE;
}
