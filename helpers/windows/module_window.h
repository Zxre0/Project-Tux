/* Bounded diagnostic lifetime, supplied by the native launcher. */
static DWORD module_window_ms(void) {
    wchar_t text[8];
    DWORD size = GetEnvironmentVariableW(L"FOL_HTTP_MODULE_MS", text, 8);
    if (!size)
        return 20000;
    if (size >= 8)
        return 0;
    DWORD value = 0;
    for (DWORD i = 0; i < size; ++i) {
        if (text[i] < L'0' || text[i] > L'9')
            return 0;
        value = value * 10 + (DWORD)(text[i] - L'0');
    }
    return value >= 1000 && value <= 300000 ? value : 0;
}
