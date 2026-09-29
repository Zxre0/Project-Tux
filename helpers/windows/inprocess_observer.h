/* Original bounded observer for the hash-gated, network-isolated child.
 * Replace one verified HTTP virtual-method pointer, forward every call unchanged,
 * then restore it. No instruction, URL, authentication or certificate changes. */
typedef BYTE(WINAPI *request_function)(void *);
static request_function original_request;
static ULONG_PTR expected_table;
static DWORD string_offset;
static LONG observation_lock, observation_enabled;
static unsigned observed_count, rejected_count;
static const char *observed_origins[8], *observed_routes[8];
static wchar_t observed_url[4096]; // Only used while holding observation_lock.

static BOOL module_read(ULONG_PTR address, void *output, SIZE_T size) {
    SIZE_T read;
    return address &&
           ReadProcessMemory(GetCurrentProcess(), (void *)address, output, size, &read) &&
           read == size;
}
static BOOL module_starts(const wchar_t *text, const wchar_t *prefix) {
    while (*prefix)
        if (*text++ != *prefix++)
            return FALSE;
    return TRUE;
}
static BOOL classify_request(void *object) {
    ULONG_PTR table;
    struct {
        ULONG_PTR data;
        LONG count, capacity;
    } value;
    ULONG_PTR address = (ULONG_PTR)object;
    if (!module_read(address, &table, sizeof(table)) || table != expected_table ||
        address + string_offset < address ||
        !module_read(address + string_offset, &value, sizeof(value)) || value.count < 1 ||
        value.count > 4096 || value.capacity < value.count || value.capacity > 1024 * 1024 ||
        !module_read(value.data, observed_url, (SIZE_T)value.count * sizeof(wchar_t)) ||
        observed_url[value.count - 1] != 0)
        return FALSE;
    const wchar_t *host;
    if (module_starts(observed_url, L"https://"))
        host = observed_url + 8;
    else if (module_starts(observed_url, L"http://"))
        host = observed_url + 7;
    else
        return FALSE;
    const wchar_t *path = host;
    while (*path && *path != L'/' && *path != L'?' && *path != L'#')
        ++path;
    const char *origin = "other";
    if (module_starts(host, L"127.0.0.1:") || module_starts(host, L"localhost:"))
        origin = "loopback";
    else {
        const wchar_t suffix[] = L".epicgames.com";
        SIZE_T host_size = (SIZE_T)(path - host), suffix_size = 14;
        if (host_size >= suffix_size) {
            BOOL match = TRUE;
            for (SIZE_T i = 0; i < suffix_size; ++i)
                match &= host[host_size - suffix_size + i] == suffix[i];
            if (match)
                origin = "epic";
        }
    }
    const char *route = "other";
    if (module_starts(path, L"/account/api/oauth/token"))
        route = "oauth-token";
    else if (module_starts(path, L"/account/api/public/"))
        route = "account-public";
    else if (module_starts(path, L"/fortnite/api/game/v2/profile/"))
        route = "fortnite-profile";
    else if (module_starts(path, L"/fortnite/api/version"))
        route = "fortnite-version";
    else if (module_starts(path, L"/lightswitch/"))
        route = "lightswitch";
    else if (module_starts(path, L"/launcher/"))
        route = "launcher";
    else if (module_starts(path, L"/content/"))
        route = "content";
    observed_origins[observed_count] = origin;
    observed_routes[observed_count++] = route;
    return TRUE;
}
static BYTE WINAPI request_observation(void *object) {
    if (InterlockedCompareExchange(&observation_lock, 1, 0) == 0) {
        if (observation_enabled && observed_count + rejected_count < 8) {
            if (!classify_request(object))
                ++rejected_count;
            // Clear transient URL contents even on a rejected descriptor.
            volatile wchar_t *clear = observed_url;
            for (unsigned i = 0; i < 4096; ++i)
                clear[i] = 0;
        }
        InterlockedExchange(&observation_lock, 0);
    }
    return original_request(object); // Always preserve the original result.
}
static BOOL module_layout(DWORD *layout, ULONG_PTR base, DWORD image_size) {
    wchar_t image_path[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, image_path, MAX_PATH);
    if (!length || length >= MAX_PATH)
        return FALSE;
    const wchar_t *name = image_path;
    for (DWORD i = 0; i < length; ++i)
        if (image_path[i] == L'\\' || image_path[i] == L'/')
            name = image_path + i + 1;
    const wchar_t fixture_name[] = L"fol-http-fixture.exe";
    if (module_starts(name, fixture_name) &&
        name[sizeof(fixture_name) / sizeof(wchar_t) - 1] == 0) {
        ULONG_PTR table = (ULONG_PTR)GetProcAddress((HMODULE)base, "synthetic_table");
        ULONG_PTR function;
        if (!table || table < base || table - base >= image_size ||
            !module_read(table, &function, sizeof(function)) || function < base ||
            function - base >= image_size)
            return FALSE;
        layout[0] = (DWORD)(function - base);
        layout[1] = (DWORD)(table - base);
        layout[2] = 104;
        layout[3] = 0;
        return TRUE;
    }
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
        layout[i] = value;
        if (i < 3 ? *p++ != L',' : *p != 0)
            return FALSE;
    }
    return layout[0] < image_size && layout[1] < image_size && layout[2] < 4096 && layout[3] < 64 &&
           layout[1] % 8 == 0 && (ULONGLONG)layout[1] + (layout[3] + 1) * 8 <= image_size;
}
static char observation_report[2048];
static unsigned observation_size;
static BOOL observation_text(const char *text) {
    while (*text) {
        if (observation_size >= sizeof(observation_report))
            return FALSE;
        observation_report[observation_size++] = *text++;
    }
    return TRUE;
}
static void observation_number(DWORD value) {
    char digits[12];
    unsigned n = 0;
    do {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    while (n) {
        char c[] = {digits[--n], 0};
        observation_text(c);
    }
}
static BOOL section_role(ULONG_PTR base, const IMAGE_DOS_HEADER *dos, const IMAGE_NT_HEADERS64 *pe,
                         DWORD rva, SIZE_T size, BOOL executable) {
    if (!pe->FileHeader.NumberOfSections || pe->FileHeader.NumberOfSections > 96)
        return FALSE;
    ULONG_PTR sections = base + dos->e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
                         pe->FileHeader.SizeOfOptionalHeader;
    for (unsigned i = 0; i < pe->FileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER section;
        if (!module_read(sections + i * sizeof(section), &section, sizeof(section)))
            return FALSE;
        if (rva >= section.VirtualAddress &&
            (ULONGLONG)rva + size <= (ULONGLONG)section.VirtualAddress + section.Misc.VirtualSize)
            return (section.Characteristics & IMAGE_SCN_MEM_READ) &&
                   ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) == executable;
    }
    return FALSE;
}
static DWORD observe_inprocess(const wchar_t *report_path, const wchar_t *session) {
    ULONG_PTR base = (ULONG_PTR)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS64 pe;
    DWORD layout[4];
    BOOL installed = FALSE, restored = FALSE;
    DWORD table_protection = 0, code_protection = 0;
    const char *error = "image-rejected";
    if (!module_read(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < 0 || dos.e_lfanew > 1024 * 1024 ||
        !module_read(base + dos.e_lfanew, &pe, sizeof(pe)) || pe.Signature != IMAGE_NT_SIGNATURE ||
        pe.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        pe.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        goto finish;
    error = "layout-rejected";
    if (!module_layout(layout, base, pe.OptionalHeader.SizeOfImage))
        goto finish;
    error = "section-rejected";
    if (!section_role(base, &dos, &pe, layout[1] + layout[3] * 8, sizeof(void *), FALSE) ||
        !section_role(base, &dos, &pe, layout[0], 1, TRUE))
        goto finish;
    expected_table = base + layout[1];
    string_offset = layout[2];
    void *volatile *slot = (void *volatile *)(expected_table + layout[3] * 8);
    ULONG_PTR target;
    MEMORY_BASIC_INFORMATION table_page, code_page;
    error = "pointer-rejected";
    if (!module_read((ULONG_PTR)slot, &target, sizeof(target)) || target != base + layout[0] ||
        !VirtualQuery((void *)slot, &table_page, sizeof(table_page)) ||
        !VirtualQuery((void *)target, &code_page, sizeof(code_page)))
        goto finish;
    error = "page-rejected";
    table_protection = table_page.Protect;
    code_protection = code_page.Protect;
    if (table_page.State != MEM_COMMIT || code_page.State != MEM_COMMIT ||
        (table_page.Protect != PAGE_READONLY && table_page.Protect != PAGE_READWRITE &&
         table_page.Protect != PAGE_WRITECOPY && table_page.Protect != PAGE_EXECUTE_WRITECOPY) ||
        (code_page.Protect != PAGE_EXECUTE_READ && code_page.Protect != PAGE_EXECUTE_READWRITE &&
         code_page.Protect != PAGE_EXECUTE_WRITECOPY))
        goto finish;
    original_request = (request_function)target;
    DWORD old;
    error = "protect-failed";
    // Real 12.50 under Proton reports EXECUTE_WRITECOPY even for .rdata.
    // Section validation above proves this slot is data; preserve existing
    // execute permission while changing only the aligned method pointer.
    DWORD writable_protection =
        table_protection == PAGE_EXECUTE_WRITECOPY ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
    if (!VirtualProtect((void *)slot, sizeof(void *), writable_protection, &old))
        goto finish;
    observation_enabled = 1;
    installed = InterlockedCompareExchangePointer(slot, (void *)request_observation,
                                                  (void *)target) == (void *)target;
    DWORD ignored;
    BOOL protection_restored = VirtualProtect((void *)slot, sizeof(void *), old, &ignored);
    if (!installed) {
        error = "pointer-changed";
        goto finish;
    }
    if (protection_restored)
        Sleep(20000);
    BOOL writable = VirtualProtect((void *)slot, sizeof(void *), writable_protection, &ignored);
    if (writable) {
        restored =
            InterlockedCompareExchangePointer(slot, (void *)target, (void *)request_observation) ==
            (void *)request_observation;
        restored &= VirtualProtect((void *)slot, sizeof(void *), old, &ignored);
    }
    while (InterlockedCompareExchange(&observation_lock, 1, 0) != 0)
        Sleep(0);
    observation_enabled = 0;
    InterlockedExchange(&observation_lock, 0);
    error = !restored              ? "restore-failed"
            : !protection_restored ? "protection-failed"
            : rejected_count       ? "request-shape-rejected"
            : observed_count       ? "none"
                                   : "no-requests";
finish:
    // The DLL stays mapped until owned-child shutdown, including late callbacks.
    observation_text("{\"event\":\"module-observation\",\"session_id\":\"");
    for (unsigned i = 0; session[i]; ++i) {
        char c[] = {(char)session[i], 0};
        observation_text(c);
    }
    observation_text("\",\"process_id\":");
    observation_number(GetCurrentProcessId());
    observation_text(",\"installed\":");
    observation_text(installed ? "true" : "false");
    observation_text(",\"restored\":");
    observation_text(restored ? "true" : "false");
    observation_text(",\"rejected\":");
    observation_number(rejected_count);
    observation_text(",\"table_protection\":");
    observation_number(table_protection);
    observation_text(",\"code_protection\":");
    observation_number(code_protection);
    observation_text(",\"error\":\"");
    observation_text(error);
    observation_text("\",\"requests\":[");
    for (unsigned i = 0; i < observed_count; ++i) {
        if (i)
            observation_text(",");
        observation_text("{\"origin\":\"");
        observation_text(observed_origins[i]);
        observation_text("\",\"route\":\"");
        observation_text(observed_routes[i]);
        observation_text("\"}");
    }
    observation_text("]}\n");
    static wchar_t path[4096];
    unsigned n = 0;
    while (report_path[n] && n < 4086) {
        path[n] = report_path[n];
        ++n;
    }
    if (report_path[n])
        return 68;
    const wchar_t suffix[] = L".requests";
    for (unsigned i = 0; i < sizeof(suffix) / sizeof(wchar_t); ++i)
        path[n + i] = suffix[i];
    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 68;
    DWORD written;
    BOOL okay = WriteFile(file, observation_report, observation_size, &written, NULL) &&
                written == observation_size && FlushFileBuffers(file);
    CloseHandle(file);
    return okay && installed && restored && observed_count && !rejected_count ? 0 : 68;
}
