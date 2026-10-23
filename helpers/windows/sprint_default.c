/* Sprint by default for builds that predate the game's own option (3.1).
 * Loaded into a player's game only.
 *
 * While the player moves, the controller's forward-movement handler starts
 * sprinting again while its "holding sprint" flag is set and Toggle Sprint is off, unless
 * the game blocks it (aiming, building, reloading...). This replaces that
 * handler in the match controller's virtual table and sets the flag first, as
 * if the sprint key were held; sprinting itself still runs through the game's
 * own ability, so the server sees it as usual. The handler reads the pawn
 * without checking it once the flag is set (the game never holds sprint without
 * one), so the flag is cleared while there is no pawn.
 *
 *   FOL_SPRINT_LAYOUT  function,table,slot,pawn,holding,toggle
 *                      RVA of the handler, RVA of the virtual table and its
 *                      slot, then the controller's MyFortPawn, bHoldingSprint
 *                      and bSprintToggleable offsets (sprint_layout in
 *                      profiles/fortnite/<version>.toml, from the exact build) */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int _fltused; // The handler takes a float; no CRT to define this.

typedef void (*move_function)(BYTE *, float);
static move_function original_move;
static DWORD pawn_offset, holding_offset, toggle_offset;

static void sprint_move(BYTE *controller, float value) {
    if (!controller[toggle_offset]) // Toggle Sprint players keep their toggle.
        controller[holding_offset] = *(void **)(controller + pawn_offset) != NULL;
    original_move(controller, value);
}

static BOOL sprint_read(ULONG_PTR address, void *output, SIZE_T size) {
    SIZE_T read;
    return address && ReadProcessMemory(GetCurrentProcess(), (void *)address, output, size, &read) &&
           read == size;
}

static BOOL sprint_layout(DWORD *layout) {
    wchar_t text[128];
    DWORD n = GetEnvironmentVariableW(L"FOL_SPRINT_LAYOUT", text, 128);
    if (!n || n >= 128)
        return FALSE;
    const wchar_t *p = text;
    for (unsigned i = 0; i < 6; ++i) {
        DWORD value = 0;
        if (*p < L'0' || *p > L'9')
            return FALSE;
        while (*p >= L'0' && *p <= L'9') {
            if (value > 26843545)
                return FALSE;
            value = value * 10 + (DWORD)(*p++ - L'0');
        }
        layout[i] = value;
        if (i < 5 ? *p++ != L',' : *p != 0)
            return FALSE;
    }
    return layout[1] % 8 == 0 && layout[2] < 4096 && layout[3] < 0x10000 && layout[4] < 0x10000 &&
           layout[5] < 0x10000 && layout[4] != layout[5];
}

/* TRUE if [rva, rva + size) lies in one readable section that is (or is not) executable. */
static BOOL sprint_section(ULONG_PTR base, const IMAGE_DOS_HEADER *dos, const IMAGE_NT_HEADERS64 *pe,
                           DWORD rva, SIZE_T size, BOOL executable) {
    if (!pe->FileHeader.NumberOfSections || pe->FileHeader.NumberOfSections > 96)
        return FALSE;
    ULONG_PTR sections = base + dos->e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
                         pe->FileHeader.SizeOfOptionalHeader;
    for (unsigned i = 0; i < pe->FileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER section;
        if (!sprint_read(sections + i * sizeof(section), &section, sizeof(section)))
            return FALSE;
        if (rva >= section.VirtualAddress &&
            (ULONGLONG)rva + size <= (ULONGLONG)section.VirtualAddress + section.Misc.VirtualSize)
            return (section.Characteristics & IMAGE_SCN_MEM_READ) &&
                   ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) == executable;
    }
    return FALSE;
}

static BOOL sprint_install(void) {
    ULONG_PTR base = (ULONG_PTR)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS64 pe;
    DWORD layout[6];
    if (!sprint_read(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < 0 || dos.e_lfanew > 1024 * 1024 ||
        !sprint_read(base + dos.e_lfanew, &pe, sizeof(pe)) || pe.Signature != IMAGE_NT_SIGNATURE ||
        pe.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        pe.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || !sprint_layout(layout) ||
        !sprint_section(base, &dos, &pe, layout[0], 1, TRUE) ||
        !sprint_section(base, &dos, &pe, layout[1] + layout[2] * 8, sizeof(void *), FALSE))
        return FALSE;
    void *volatile *slot = (void *volatile *)(base + layout[1] + layout[2] * 8);
    ULONG_PTR target;
    if (!sprint_read((ULONG_PTR)slot, &target, sizeof(target)) || target != base + layout[0])
        return FALSE;
    pawn_offset = layout[3];
    holding_offset = layout[4];
    toggle_offset = layout[5];
    original_move = (move_function)target;
    MEMORY_BASIC_INFORMATION page;
    if (!VirtualQuery((void *)slot, &page, sizeof(page)) || page.State != MEM_COMMIT)
        return FALSE;
    // The table is data; keep execute permission where the self-unpacking image has it.
    DWORD writable = page.Protect == PAGE_EXECUTE_WRITECOPY || page.Protect == PAGE_EXECUTE_READWRITE
                         ? PAGE_EXECUTE_READWRITE
                         : PAGE_READWRITE;
    DWORD old, ignored;
    if (!VirtualProtect((void *)slot, sizeof(void *), writable, &old))
        return FALSE;
    BOOL installed = InterlockedCompareExchangePointer(slot, (void *)sprint_move, (void *)target) ==
                     (void *)target;
    VirtualProtect((void *)slot, sizeof(void *), old, &ignored);
    return installed;
}

static DWORD WINAPI sprint_worker(void *unused) {
    (void)unused;
    // The early loader runs while the image may still be unpacking; keep trying for a minute.
    for (unsigned attempt = 0; attempt < 60; ++attempt) {
        if (sprint_install())
            return 0;
        Sleep(1000);
    }
    return 64;
}

BOOL WINAPI sprint_entry(HINSTANCE module, DWORD reason, void *reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(NULL, 0, sprint_worker, NULL, 0, NULL);
        if (!thread)
            return FALSE;
        CloseHandle(thread);
    }
    return TRUE;
}
