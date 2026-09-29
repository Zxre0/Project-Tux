/* Read only the explicitly selected generated local-login JSON. Never accept
 * a production account or log credentials. Append ordinary game login arguments. */
static BOOL login_space(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }
static BOOL login_string(const char **cursor, char *value, unsigned capacity) {
    const char *p = *cursor;
    if (*p++ != '"')
        return FALSE;
    unsigned n = 0;
    while (*p && *p != '"') {
        if ((unsigned char)*p < 32 || *p == '\\' || n + 1 >= capacity)
            return FALSE;
        value[n++] = *p++;
    }
    if (*p++ != '"')
        return FALSE;
    value[n] = 0;
    *cursor = p;
    return TRUE;
}
static BOOL login_equal(const char *a, const char *b) {
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}
static wchar_t *development_command(const wchar_t *original, BOOL *requested) {
    static wchar_t path[4096];
    DWORD path_size = GetEnvironmentVariableW(L"FOL_DEV_LOGIN_FILE", path, 4096);
    char username[64] = {0}, password[64] = {0};
    if (path_size) {
        if (path_size >= 4096)
            return NULL;
        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE)
            return NULL;
        char data[512];
        DWORD size = 0;
        BOOL read = ReadFile(file, data, sizeof(data) - 1, &size, NULL);
        LARGE_INTEGER file_size;
        BOOL bounded = GetFileSizeEx(file, &file_size) && file_size.QuadPart == size;
        CloseHandle(file);
        if (!read || !bounded || !size || size >= sizeof(data) - 1)
            return NULL;
        data[size] = 0;
        const char *p = data;
        while (login_space(*p))
            ++p;
        if (*p++ != '{')
            return NULL;
        BOOL have_user = FALSE, have_password = FALSE;
        for (unsigned i = 0; i < 2; ++i) {
            while (login_space(*p))
                ++p;
            char key[16];
            if (!login_string(&p, key, sizeof(key)))
                return NULL;
            while (login_space(*p))
                ++p;
            if (*p++ != ':')
                return NULL;
            while (login_space(*p))
                ++p;
            if (login_equal(key, "username") && !have_user) {
                if (!login_string(&p, username, sizeof(username)))
                    return NULL;
                have_user = TRUE;
            } else if (login_equal(key, "password") && !have_password) {
                if (!login_string(&p, password, sizeof(password)))
                    return NULL;
                have_password = TRUE;
            } else
                return NULL;
            while (login_space(*p))
                ++p;
            if (i == 0 ? *p++ != ',' : *p++ != '}')
                return NULL;
        }
        while (login_space(*p))
            ++p;
        if (*p || p != data + size || !have_user || !have_password)
            return NULL;
        unsigned name_size = 0;
        while (username[name_size] && username[name_size] != '@') {
            char c = username[name_size++];
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
                return NULL;
        }
        if (!name_size || name_size > 32 || !login_equal(username + name_size, "@localhost"))
            return NULL;
        unsigned password_size = 0;
        while (password[password_size]) {
            char c = password[password_size++];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                return NULL;
        }
        if (password_size != 32)
            return NULL;
        memset(data, 0, sizeof(data));
        *requested = TRUE;
    }
    SIZE_T count = length(original), capacity = count + 192;
    wchar_t *command = HeapAlloc(GetProcessHeap(), 0, capacity * sizeof(wchar_t));
    if (!command)
        return NULL;
    for (SIZE_T i = 0; i < count; ++i)
        command[i] = original[i];
    if (*requested) {
        const char *parts[] = {" -AUTH_LOGIN=", username, " -AUTH_PASSWORD=", password,
                               " -AUTH_TYPE=epic"};
        for (unsigned i = 0; i < 5; ++i)
            for (unsigned j = 0; parts[i][j]; ++j)
                command[count++] = (wchar_t)parts[i][j];
    }
    command[count] = 0;
    memset(password, 0, sizeof(password));
    return command;
}
