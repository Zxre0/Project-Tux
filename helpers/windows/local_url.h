/* Original private-development URL selection. No configurable remote endpoint.
 * Returns 1 for a selected local rewrite, 0 for an unrelated URL, -1 if malformed
 * or too large. No allocation, I/O, authentication or certificate manipulation. */
static int fol_wide_prefix(const wchar_t *text, const wchar_t *prefix) {
    while (*prefix)
        if (*text++ != *prefix++)
            return 0;
    return 1;
}
static int fol_local_url(const wchar_t *input, wchar_t *output, unsigned capacity) {
    if (!input || !output || !capacity)
        return -1;
    output[0] = 0;
    unsigned length = 0;
    while (length < 4096 && input[length]) {
        if (input[length] < 32 || input[length] == 127 || input[length] == L'#' ||
            input[length] == L'\\')
            return -1;
        ++length;
    }
    if (!length || length == 4096)
        return -1;
    if (!fol_wide_prefix(input, L"https://"))
        return 0;
    const wchar_t *host = input + 8, *path = host;
    while (*path && *path != L'/') {
        if (!((*path >= L'a' && *path <= L'z') || (*path >= L'0' && *path <= L'9') ||
              *path == L'-' || *path == L'.'))
            return -1;
        ++path;
    }
    if (path == host || *path != L'/')
        return -1;
    const wchar_t *suffixes[] = {L".ol.epicgames.com", L".ol.epicgames.net"};
    int selected = 0;
    unsigned host_length = (unsigned)(path - host);
    for (unsigned i = 0; i < 2; ++i) {
        const wchar_t *suffix = suffixes[i];
        unsigned size = 0;
        while (suffix[size])
            ++size;
        if (host_length > size) {
            int match = 1;
            for (unsigned j = 0; j < size; ++j)
                match &= host[host_length - size + j] == suffix[j];
            selected |= match;
        }
    }
    if (!selected)
        return 0;
    if (!(fol_wide_prefix(path, L"/account/api/") || fol_wide_prefix(path, L"/fortnite/api/") ||
          fol_wide_prefix(path, L"/lightswitch/api/") || fol_wide_prefix(path, L"/content/api/") ||
          fol_wide_prefix(path, L"/launcher/api/") ||
          fol_wide_prefix(path, L"/party/api/v1/Fortnite/") ||
          fol_wide_prefix(path, L"/api/v1/events/Fortnite/") ||
          fol_wide_prefix(path, L"/api/v1/leaderboards/Fortnite/")))
        return 0;
    const wchar_t local[] = L"http://127.0.0.1:3551";
    unsigned prefix_size = sizeof(local) / sizeof(wchar_t) - 1;
    unsigned path_size = length - (unsigned)(path - input);
    if (prefix_size + path_size + 1 > capacity)
        return -1;
    for (unsigned i = 0; i < prefix_size; ++i)
        output[i] = local[i];
    for (unsigned i = 0; i <= path_size; ++i)
        output[prefix_size + i] = path[i];
    return 1;
}
