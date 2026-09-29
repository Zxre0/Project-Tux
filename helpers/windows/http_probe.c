/* Default: read-only interface discovery in one owned child. The separately
 * compiled observer variant uses debugging registers to observe requests.
 * The separately compiled module bootstrap loads one explicitly supplied project
 * DLL into its own child using a fresh allocation for the DLL path. None accepts
 * arbitrary PIDs or changes authentication, certificate validation or security checks. */
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

static HANDLE report = INVALID_HANDLE_VALUE;
static BOOL inspect_code;
#ifdef FOL_REQUEST_OBSERVER
static DWORD observed_function, observed_reference;
#endif
static void output(const char *text) {
    static char line[4096];
    static DWORD size;
    while (*text) {
        if (size == sizeof(line))
            ExitProcess(68);
        line[size++] = *text;
        if (*text++ == '\n') {
            DWORD written;
            if (!WriteFile(report, line, size, &written, NULL) || written != size)
                ExitProcess(68);
            size = 0;
        }
    }
}
static void number(ULONGLONG value) {
    char digits[24];
    unsigned n = 0;
    do {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    while (n) {
        char digit[2] = {digits[--n], 0};
        output(digit);
    }
}
static void event(const char *name, DWORD code) {
    output("FOL_HTTP_PROBE {\"event\":\"");
    output(name);
    output("\",\"value\":");
    number(code);
    output("}\n");
}
static BOOL equal(const wchar_t *a, const wchar_t *b) {
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}
static SIZE_T length(const wchar_t *s) {
    SIZE_T n = 0;
    while (s[n])
        ++n;
    return n;
}
static wchar_t *argument(wchar_t *line, wchar_t *word) {
    while (*line == L' ' || *line == L'\t')
        ++line;
    BOOL quoted = *line == L'"';
    if (quoted)
        ++line;
    unsigned n = 0;
    while (*line && (quoted ? *line != L'"' : (*line != L' ' && *line != L'\t'))) {
        if (n >= MAX_PATH - 1 || *line == L'"')
            return NULL;
        word[n++] = *line++;
    }
    word[n] = 0;
    if (!n || (quoted && *line++ != L'"'))
        return NULL;
    if (*line && *line != L' ' && *line != L'\t')
        return NULL;
    while (*line == L' ' || *line == L'\t')
        ++line;
    return line;
}
static BOOL read_exact(HANDLE process, ULONG_PTR address, void *data, SIZE_T size) {
    SIZE_T read = 0;
    return ReadProcessMemory(process, (void *)address, data, size, &read) && read == size;
}
static BOOL readable(DWORD protection) {
    if (protection & (PAGE_GUARD | PAGE_NOACCESS))
        return FALSE;
    protection &= 0xff;
    return protection == PAGE_READONLY || protection == PAGE_READWRITE ||
           protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ ||
           protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}
#ifdef FOL_REQUEST_OBSERVER
#include "request_observer.h"
#endif
static ULONG_PTR image_base(DWORD pid, const wchar_t *name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;
    MODULEENTRY32W item = {0};
    item.dwSize = sizeof(item);
    ULONG_PTR base = 0;
    if (Module32FirstW(snapshot, &item)) {
        do {
            if (equal(item.szModule, name)) {
                base = (ULONG_PTR)item.modBaseAddr;
                break;
            }
        } while (Module32NextW(snapshot, &item));
    }
    CloseHandle(snapshot);
    return base;
}
#ifdef FOL_MODULE_BOOTSTRAP
#include "module_loader.h"
#endif
static BOOL in_range(DWORD offset, SIZE_T size, DWORD image_size) {
    return offset <= image_size && size <= image_size - offset;
}
static BOOL valid_span(const BYTE *valid, DWORD offset, SIZE_T count, DWORD size) {
    if (!in_range(offset, count, size))
        return FALSE;
    for (SIZE_T i = 0; i < count; ++i)
        if (!valid[offset + i])
            return FALSE;
    return TRUE;
}
static BOOL code_pointer(HANDLE child, ULONG_PTR base, const BYTE *image, const BYTE *valid,
                         DWORD size, DWORD slot) {
    if (!valid_span(valid, slot, sizeof(ULONG_PTR), size))
        return FALSE;
    ULONG_PTR target = *(const ULONG_PTR *)(image + slot);
    if (target < base || target - base >= size)
        return FALSE;
    MEMORY_BASIC_INFORMATION page;
    if (!VirtualQueryEx(child, (void *)target, &page, sizeof(page)) || page.State != MEM_COMMIT ||
        !readable(page.Protect))
        return FALSE;
    DWORD protection = page.Protect & 0xff;
    return protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE ||
           protection == PAGE_EXECUTE_WRITECOPY;
}
static void pointer_table(HANDLE child, ULONG_PTR base, const BYTE *image, const BYTE *valid,
                          DWORD size, DWORD reference) {
    DWORD begin = reference, end = reference + 8;
    unsigned before = 0, after = 0;
    while (begin >= 8 && before < 64 && code_pointer(child, base, image, valid, size, begin - 8)) {
        begin -= 8;
        ++before;
    }
    while (after < 64 && code_pointer(child, base, image, valid, size, end)) {
        end += 8;
        ++after;
    }
    /* A contiguous code-pointer table is not by itself a C++ vtable. Report the
     * observed bounds and executable references; never invoke any candidate. */
    output("FOL_HTTP_PROBE {\"event\":\"pointer_table\",\"reference_rva\":");
    number(reference);
    output(",\"start_rva\":");
    number(begin);
    output(",\"candidate_slot\":");
    number(before);
    output(",\"scan_limit_reached\":");
    output(before == 64 || after == 64 ? "true" : "false");
    output(",\"function_rvas\":[");
    for (DWORD slot = begin; slot < end; slot += 8) {
        if (slot != begin)
            output(",");
        number(*(const ULONG_PTR *)(image + slot) - base);
    }
    output("],\"table_address_xrefs\":[");
    unsigned refs = 0;
    for (DWORD i = 0; i + 7 <= size && refs < 64; ++i) {
        if ((image[i] != 0x48 && image[i] != 0x4c) || image[i + 1] != 0x8d ||
            (image[i + 2] & 0xc7) != 0x05 || !valid_span(valid, i, 7, size))
            continue;
        LONG displacement;
        for (unsigned j = 0; j < 4; ++j)
            ((BYTE *)&displacement)[j] = image[i + 3 + j];
        LONGLONG target = (LONGLONG)i + 7 + displacement;
        if (target < begin || target >= end || (target - begin) % 8)
            continue;
        MEMORY_BASIC_INFORMATION page;
        if (!VirtualQueryEx(child, (void *)(base + i), &page, sizeof(page)) ||
            !(page.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            continue;
        if (refs++)
            output(",");
        output("{\"instruction_rva\":");
        number(i);
        output(",\"table_slot\":");
        number((target - begin) / 8);
        output("}");
    }
    output("]}\n");
    if (inspect_code) {
        for (DWORD slot = begin; slot < end; slot += 8) {
            DWORD target = (DWORD)(*(const ULONG_PTR *)(image + slot) - base);
            if (!valid_span(valid, target, 128, size))
                continue;
            output("FOL_HTTP_PROBE {\"event\":\"code_observation\",\"function_rva\":");
            number(target);
            output(",\"table_slot\":");
            number((slot - begin) / 8);
            output(",\"bytes\":\"");
            for (DWORD i = 0; i < 128; ++i) {
                static const char hex[] = "0123456789abcdef";
                char byte[3] = {hex[image[target + i] >> 4], hex[image[target + i] & 15], 0};
                output(byte);
            }
            output("\"}\n");
        }
    }
}
static BOOL scan(HANDLE child, ULONG_PTR base) {
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS64 nt;
    if (!read_exact(child, base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < 0 || dos.e_lfanew > 1024 * 1024 ||
        !read_exact(child, base + dos.e_lfanew, &nt, sizeof(nt)) ||
        nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return FALSE;
    DWORD size = nt.OptionalHeader.SizeOfImage;
    if (size < 4096 || size > 256 * 1024 * 1024)
        return FALSE;
    BYTE *image = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
    BYTE *valid = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
    if (!image || !valid) {
        if (image)
            HeapFree(GetProcessHeap(), 0, image);
        if (valid)
            HeapFree(GetProcessHeap(), 0, valid);
        return FALSE;
    }
    /* Never read guard/no-access regions, and never write any process memory.
     * Validity bytes prevent unread pages from manufacturing zero matches. */
    DWORD offset = 0;
    while (offset < size) {
        MEMORY_BASIC_INFORMATION region;
        if (!VirtualQueryEx(child, (void *)(base + offset), &region, sizeof(region)))
            break;
        ULONG_PTR end = (ULONG_PTR)region.BaseAddress + region.RegionSize;
        if (end <= base + offset)
            break;
        SIZE_T count = end - base - offset;
        if (count > size - offset)
            count = size - offset;
        if (region.State == MEM_COMMIT && readable(region.Protect) &&
            read_exact(child, base + offset, image + offset, count)) {
            for (SIZE_T i = 0; i < count; ++i)
                valid[offset + i] = 1;
        }
        offset += (DWORD)count;
    }
    const wchar_t marker[] = L"STAT_FCurlHttpRequest_ProcessRequest";
    DWORD marker_rva = 0, marker_count = 0;
    for (DWORD i = 0; i + sizeof(marker) <= size; i += 2) {
        BOOL match = TRUE;
        for (SIZE_T j = 0; j < sizeof(marker); ++j) {
            if (!valid[i + j] || image[i + j] != ((const BYTE *)marker)[j]) {
                match = FALSE;
                break;
            }
        }
        if (match) {
            marker_rva = i;
            ++marker_count;
        }
    }
    event("marker_matches", marker_count);
    BOOL found = FALSE;
    if (marker_count == 1) {
        const IMAGE_DATA_DIRECTORY exception =
            nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        DWORD xrefs = 0;
        for (DWORD i = 0; i + 7 <= size; ++i) {
            /* x64 LEA reg,[RIP+disp32]; only an observation, not a signature
             * authorizing a hook. The exception directory supplies function bounds. */
            if (!valid_span(valid, i, 7, size) || (image[i] != 0x48 && image[i] != 0x4c) ||
                image[i + 1] != 0x8d || (image[i + 2] & 0xc7) != 0x05)
                continue;
            LONG displacement;
            BYTE *d = (BYTE *)&displacement;
            for (unsigned j = 0; j < 4; ++j)
                d[j] = image[i + 3 + j];
            if ((LONGLONG)i + 7 + displacement != marker_rva)
                continue;
            MEMORY_BASIC_INFORMATION code;
            if (!VirtualQueryEx(child, (void *)(base + i), &code, sizeof(code)))
                continue;
            DWORD protection = code.Protect & 0xff;
            if (protection != PAGE_EXECUTE_READ && protection != PAGE_EXECUTE_READWRITE &&
                protection != PAGE_EXECUTE_WRITECOPY)
                continue;
            ++xrefs;
            DWORD begin = 0, end = 0;
            if (in_range(exception.VirtualAddress, exception.Size, size)) {
                for (DWORD j = 0; j + 12 <= exception.Size; j += 12) {
                    DWORD *entry = (DWORD *)(image + exception.VirtualAddress + j);
                    if (valid[exception.VirtualAddress + j] &&
                        valid[exception.VirtualAddress + j + 11] && entry[0] <= i && i < entry[1] &&
                        entry[1] <= size) {
                        begin = entry[0];
                        end = entry[1];
                        break;
                    }
                }
            }
            output("FOL_HTTP_PROBE {\"event\":\"candidate\",\"xref_rva\":");
            number(i);
            output(",\"function_begin_rva\":");
            number(begin);
            output(",\"function_end_rva\":");
            number(end);
            output(",\"pointer_references\":[");
            unsigned pointers = 0;
            if (begin) {
                for (DWORD j = 0; j + sizeof(ULONG_PTR) <= size; j += sizeof(ULONG_PTR)) {
                    if (valid_span(valid, j, 8, size) &&
                        *(ULONG_PTR *)(image + j) == base + begin) {
                        if (pointers < 32) {
                            if (pointers)
                                output(",");
                            number(j);
                        }
                        ++pointers;
                    }
                }
            }
            output("],\"pointer_reference_count\":");
            number(pointers);
            output("}\n");
            if (begin && pointers == 1) {
                for (DWORD j = 0; j + sizeof(ULONG_PTR) <= size; j += sizeof(ULONG_PTR)) {
                    if (valid_span(valid, j, 8, size) &&
                        *(ULONG_PTR *)(image + j) == base + begin) {
                        pointer_table(child, base, image, valid, size, j);
#ifdef FOL_REQUEST_OBSERVER
                        observed_function = begin;
                        observed_reference = j;
#endif
                        break;
                    }
                }
            }
            found |= begin != 0 && pointers != 0;
            if (xrefs >= 32)
                break;
        }
        event("xref_matches_up_to_limit", xrefs);
    }
    HeapFree(GetProcessHeap(), 0, valid);
    HeapFree(GetProcessHeap(), 0, image);
    return found;
}

/* Freestanding PE entry point; Windows API only, no MSVC/MinGW runtime needed. */
void probe_entry(void) {
    static wchar_t report_path[4096];
    wchar_t session[128];
    DWORD path_size = GetEnvironmentVariableW(L"FOL_HTTP_PROBE_LOG", report_path, 4096);
    DWORD session_size = GetEnvironmentVariableW(L"FOL_SESSION_ID", session, 128);
    if (!path_size || path_size >= 4096 || !session_size || session_size >= 128)
        ExitProcess(64);
    char session_ascii[128];
    for (DWORD i = 0; i < session_size; ++i) {
        wchar_t c = session[i];
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') ||
              c == L'-' || c == L'_'))
            ExitProcess(64);
        session_ascii[i] = (char)c;
    }
    session_ascii[session_size] = 0;
    wchar_t code_option[2];
    inspect_code = GetEnvironmentVariableW(L"FOL_HTTP_PROBE_CODE", code_option, 2) == 1 &&
                   code_option[0] == L'1';
    /* CREATE_NEW prevents overwriting an earlier report. No dependence on the
     * console handles provided (or omitted) by Proton's steam.exe wrapper. */
    report = CreateFileW(report_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (report == INVALID_HANDLE_VALUE)
        ExitProcess(68);
    output("FOL_HTTP_PROBE {\"event\":\"started\",\"session_id\":\"");
    output(session_ascii);
    output("\"}\n");
    wchar_t *line = GetCommandLineW();
    /* Parse only the two executable paths; forward remaining child arguments
     * verbatim to CreateProcess, never to a shell. Windows filenames cannot
     * contain a quote. Wine may omit unnecessary surrounding quotes. */
    wchar_t self[MAX_PATH];
    line = argument(line, self);
    if (!line) {
        event("arguments_rejected", 64);
        ExitProcess(64);
    }
    wchar_t *target_command = line;
    wchar_t target[MAX_PATH];
    if (!argument(line, target)) {
        event("arguments_rejected", 64);
        ExitProcess(64);
    }
    const wchar_t *name = target;
    for (unsigned i = 0; target[i]; ++i)
        if (target[i] == L'\\' || target[i] == L'/')
            name = target + i + 1;
    if (!equal(name, L"FortniteClient-Win64-Shipping.exe") && !equal(name, L"fol-http-fixture.exe"))
        ExitProcess(64);
    HANDLE job = CreateJobObjectW(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job ||
        !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        event("job_setup_failed", GetLastError());
        ExitProcess(65);
    }
    SIZE_T command_size = (length(target_command) + 1) * sizeof(wchar_t);
    wchar_t *command = HeapAlloc(GetProcessHeap(), 0, command_size);
    if (!command)
        ExitProcess(65);
    for (SIZE_T i = 0; i < command_size / sizeof(wchar_t); ++i)
        command[i] = target_command[i];
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION child = {0};
    startup.cb = sizeof(startup);
    if (!CreateProcessW(target, command, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup,
                        &child)) {
        event("child_start_failed", GetLastError());
        ExitProcess(66);
    }
    HeapFree(GetProcessHeap(), 0, command);
    if (!AssignProcessToJobObject(job, child.hProcess)) {
        event("job_assignment_failed", GetLastError());
        TerminateProcess(child.hProcess, 66);
        ExitProcess(66);
    }
    if (ResumeThread(child.hThread) == (DWORD)-1) {
        event("child_resume_failed", GetLastError());
        CloseHandle(job);
        ExitProcess(66);
    }
    CloseHandle(child.hThread);
    event("owned_child_started", child.dwProcessId);
    BOOL discovered = FALSE;
    ULONG_PTR base = 0;
    for (unsigned attempt = 0; attempt < 10; ++attempt) {
        if (WaitForSingleObject(child.hProcess, 3000) != WAIT_TIMEOUT)
            break;
        base = image_base(child.dwProcessId, name);
        if (base && scan(child.hProcess, base)) {
            discovered = TRUE;
            break;
        }
    }
    event("interface_candidate_observed", discovered);
#ifdef FOL_REQUEST_OBSERVER
    BOOL observation = discovered && observed_function && observed_reference &&
                       observe_requests(&child, base, observed_function, observed_reference,
                                        equal(name, L"fol-http-fixture.exe"));
#elif defined(FOL_MODULE_BOOTSTRAP)
    BOOL observation = discovered && load_owned_module(&child);
#else
    BOOL observation = TRUE;
#endif
    /* End only the child job created here. These are diagnostic probes, not a
     * completed login or an authorization to patch a reported candidate. */
    CloseHandle(job);
    WaitForSingleObject(child.hProcess, 5000);
    CloseHandle(child.hProcess);
    event("completed", discovered && observation ? 0 : 67);
    if (!FlushFileBuffers(report) || !CloseHandle(report))
        ExitProcess(68);
    ExitProcess(discovered && observation ? 0 : 67);
}
