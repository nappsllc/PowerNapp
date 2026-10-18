/* lang.c - string table and language loader.
   English is built in (strings.h); other languages are UTF-8 text files <exe dir>\lang\<tag>.txt of
   "key = value" lines.  Missing/invalid keys fall back to English per string.  Self-contained: the
   saved language choice (INI key "lang") is read/written by the caller and passed to lang_resolve. */
#include <strsafe.h>
#include "strings.h"

#define LANG_TAG_MAX 16
#define LANG_NAME_MAX 48

typedef struct { wchar_t tag[LANG_TAG_MAX], name[LANG_NAME_MAX]; bool rtl; } LangInfo;

static const wchar_t *const g_lang_en[L_COUNT] = {
#define X(id, k, e) e,
    STRINGS(X)
#undef X
};
static const char *const g_lang_keys[L_COUNT] = {
#define X(id, k, e) k,
    STRINGS(X)
#undef X
};

static const wchar_t *g_lang_cur[L_COUNT];     /* active translations; NULL = use English */
static wchar_t *g_lang_blk;                    /* heap block backing g_lang_cur / last lang_parse */
static wchar_t g_lang_tag[LANG_TAG_MAX] = L"en";
static bool g_lang_rtl;

static const wchar_t *TR(StrId id)
{
    if ((unsigned)id >= (unsigned)L_COUNT) return L"";
    return g_lang_cur[id] ? g_lang_cur[id] : g_lang_en[id];
}
static bool lang_rtl(void) { return g_lang_rtl; }
static const wchar_t *lang_tag(void) { return g_lang_tag; }

/* ---- placeholders: same ORDERED sequence of %ls / %d / %lld as English ---- */
static bool lang_ph_seq(const wchar_t *s, char *seq, int cap)
{
    int n = 0;
    for (; *s; s++) {
        if (*s != L'%') continue;
        if (s[1] == L'%') { s++; continue; }
        char c;
        if (s[1] == L'l' && s[2] == L's') { c = 's'; s += 2; }
        else if (s[1] == L'l' && s[2] == L'l' && s[3] == L'd') { c = 'q'; s += 3; }
        else if (s[1] == L'd') { c = 'd'; s += 1; }
        else return false;      /* unknown conversion */
        if (n >= cap - 1) return false;
        seq[n++] = c;
    }
    seq[n] = 0;
    return true;
}
static bool lang_ph_ok(const wchar_t *en, const wchar_t *tr)
{
    char a[32], b[32];
    return lang_ph_seq(en, a, 32) && lang_ph_seq(tr, b, 32) && strcmp(a, b) == 0;
}

/* ---- parsing ---- */
/* Convert one UTF-8 span to wide, unescaping \n and \\, into dst (room for n+1). NULL on invalid UTF-8. */
static wchar_t *lang_conv(const char *s, int n, wchar_t *dst)
{
    int w = 0;
    if (n > 0) {
        w = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, n, dst, n);
        if (w <= 0) return NULL;
    }
    int o = 0;
    for (int i = 0; i < w; i++) {
        if (dst[i] == L'\\' && i + 1 < w && dst[i + 1] == L'n') { dst[o++] = L'\n'; i++; }
        else if (dst[i] == L'\\' && i + 1 < w && dst[i + 1] == L'\\') { dst[o++] = L'\\'; i++; }
        else dst[o++] = dst[i];
    }
    dst[o] = 0;
    return dst;
}

/* Parse into table (may be NULL: header only). Allocates the value block into *blk. */
static bool lang_parse_impl(const char *s, size_t len, const wchar_t **table, wchar_t *name, size_t nn,
                            bool *rtl, wchar_t **blk)
{
    bool any = false;
    wchar_t *buf = NULL, *pos = NULL;
    if (name && nn) name[0] = 0;
    if (rtl) *rtl = false;
    if (table) {
        if (len > 0x7fffffff / 2) return false;
        buf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(wchar_t));
        if (!buf) return false;
        pos = buf;
    }
    if (len >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) { s += 3; len -= 3; }
    const char *end = s + len;
    while (s < end) {
        const char *le = s;
        while (le < end && *le != '\n') le++;
        const char *a = s, *b = le;
        s = le < end ? le + 1 : le;
        while (a < b && (*a == ' ' || *a == '\t')) a++;
        while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r')) b--;
        if (a >= b || *a == '#') continue;
        const char *eq = a;
        while (eq < b && *eq != '=') eq++;
        if (eq >= b) continue;
        const char *ke = eq;
        while (ke > a && (ke[-1] == ' ' || ke[-1] == '\t')) ke--;
        const char *vs = eq + 1;
        while (vs < b && (*vs == ' ' || *vs == '\t')) vs++;
        int klen = (int)(ke - a), vlen = (int)(b - vs);
        if (klen <= 0) continue;
        if (*a == '@') {
            wchar_t tmp[LANG_NAME_MAX * 3 + 8];
            if (vlen <= 0 || vlen >= LANG_NAME_MAX) continue;
            if (!lang_conv(vs, vlen, tmp)) continue;
            if (klen == 5 && memcmp(a, "@name", 5) == 0) {
                if (name && nn && wcslen(tmp) < nn) { StringCchCopyW(name, nn, tmp); any = true; }
            } else if (klen == 4 && memcmp(a, "@rtl", 4) == 0) {
                if (rtl) *rtl = tmp[0] == L'1';
            }
            continue;
        }
        if (!table || vlen <= 0) continue;
        for (int i = 0; i < L_COUNT; i++) {
            if ((int)strlen(g_lang_keys[i]) != klen || memcmp(g_lang_keys[i], a, klen) != 0) continue;
            wchar_t *v = lang_conv(vs, vlen, pos);
            if (v && v[0] && lang_ph_ok(g_lang_en[i], v)) {
                table[i] = v;
                pos += wcslen(v) + 1;
                any = true;
            }
            break;
        }
    }
    if (blk) { *blk = buf; } else if (buf) HeapFree(GetProcessHeap(), 0, buf);
    return any;
}

/* Fills only keys that are present and valid; the others are left untouched. The values live in a
   module-owned block that is replaced by the next lang_parse/lang_load. */
static bool lang_parse(const char *utf8, size_t len, const wchar_t **table, wchar_t *name, size_t nn, bool *rtl)
{
    wchar_t *blk = NULL;
    bool ok = lang_parse_impl(utf8, len, table, name, nn, rtl, &blk);
    if (g_lang_blk) HeapFree(GetProcessHeap(), 0, g_lang_blk);
    g_lang_blk = blk;
    return ok;
}

/* ---- files ---- */
static bool lang_tag_ok(const wchar_t *t)
{
    size_t n = wcslen(t);
    if (n == 0 || n >= LANG_TAG_MAX) return false;
    for (; *t; t++)
        if (!((*t >= L'a' && *t <= L'z') || (*t >= L'A' && *t <= L'Z') || (*t >= L'0' && *t <= L'9') || *t == L'-')) return false;
    return true;
}
static void lang_dir(wchar_t *out, size_t n)
{
    wchar_t p[MAX_PATH];
    DWORD k = GetModuleFileNameW(NULL, p, MAX_PATH);
    if (k == 0 || k >= MAX_PATH) p[0] = 0;
    wchar_t *sl = wcsrchr(p, L'\\');
    if (sl) *sl = 0; else p[0] = 0;
    StringCchPrintfW(out, n, L"%ls\\lang", p);
    out[n - 1] = 0;
}
static char *lang_read_file(const wchar_t *path, size_t *len)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    LARGE_INTEGER sz;
    char *buf = NULL;
    if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && sz.QuadPart <= (1 << 20)) {
        buf = (char *)HeapAlloc(GetProcessHeap(), 0, (size_t)sz.QuadPart);
        DWORD got = 0;
        if (buf && (!ReadFile(h, buf, (DWORD)sz.QuadPart, &got, NULL) || got != (DWORD)sz.QuadPart)) {
            HeapFree(GetProcessHeap(), 0, buf); buf = NULL;
        }
        if (buf) *len = (size_t)sz.QuadPart;
    }
    CloseHandle(h);
    return buf;
}

static void lang_load(const wchar_t *tag)
{
    for (int i = 0; i < L_COUNT; i++) g_lang_cur[i] = NULL;
    if (g_lang_blk) { HeapFree(GetProcessHeap(), 0, g_lang_blk); g_lang_blk = NULL; }
    g_lang_rtl = false;
    StringCchCopyW(g_lang_tag, LANG_TAG_MAX, L"en");
    if (!tag || !lang_tag_ok(tag) || _wcsicmp(tag, L"en") == 0) return;
    wchar_t dir[MAX_PATH], path[MAX_PATH + 32];
    lang_dir(dir, MAX_PATH);
    StringCchPrintfW(path, MAX_PATH + 32, L"%ls\\%ls.txt", dir, tag);
    path[MAX_PATH + 31] = 0;
    size_t len = 0;
    char *data = lang_read_file(path, &len);
    if (!data) return;
    wchar_t name[LANG_NAME_MAX];
    bool rtl = false;
    bool ok = lang_parse(data, len, g_lang_cur, name, LANG_NAME_MAX, &rtl);
    HeapFree(GetProcessHeap(), 0, data);
    if (!ok) {
        for (int i = 0; i < L_COUNT; i++) g_lang_cur[i] = NULL;
        return;
    }
    StringCchCopyW(g_lang_tag, LANG_TAG_MAX, tag);
    g_lang_rtl = rtl;
}

/* ---- listing ---- */
static int lang_list(LangInfo *out, int max)
{
    int n = 0;
    if (max <= 0) return 0;
    StringCchCopyW(out[n].tag, LANG_TAG_MAX, L"en"); StringCchCopyW(out[n].name, LANG_NAME_MAX, L"English"); out[n].rtl = false; n++;
    wchar_t dir[MAX_PATH], pat[MAX_PATH + 8];
    lang_dir(dir, MAX_PATH);
    StringCchPrintfW(pat, MAX_PATH + 8, L"%ls\\*.txt", dir);
    pat[MAX_PATH + 7] = 0;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return n;
    int first = n;
    do {
        if (n >= max || (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        wchar_t tag[LANG_TAG_MAX];
        size_t fl = wcslen(fd.cFileName);
        if (fl < 5 || fl - 4 >= LANG_TAG_MAX) continue;
        StringCchCopyNW(tag, LANG_TAG_MAX, fd.cFileName, fl - 4); tag[fl - 4] = 0;
        if (!lang_tag_ok(tag) || _wcsicmp(tag, L"en") == 0) continue;
        wchar_t path[MAX_PATH + 32];
        StringCchPrintfW(path, MAX_PATH + 32, L"%ls\\%ls", dir, fd.cFileName);
        path[MAX_PATH + 31] = 0;
        size_t len = 0;
        char *data = lang_read_file(path, &len);
        if (!data) continue;
        LangInfo li;
        StringCchCopyW(li.tag, LANG_TAG_MAX, tag);
        bool ok = lang_parse_impl(data, len, NULL, li.name, LANG_NAME_MAX, &li.rtl, NULL);
        HeapFree(GetProcessHeap(), 0, data);
        if (!ok || !li.name[0]) continue;
        out[n++] = li;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    for (int i = first + 1; i < n; i++) {           /* insertion sort by display name */
        LangInfo t = out[i];
        int j = i - 1;
        while (j >= first && _wcsicmp(out[j].name, t.name) > 0) { out[j + 1] = out[j]; j--; }
        out[j + 1] = t;
    }
    return n;
}

/* ---- resolving ---- */
static bool lang_pick(const wchar_t **avail, int n, const wchar_t *tag, wchar_t *out, size_t on)
{
    for (int i = 0; i < n; i++)
        if (_wcsicmp(avail[i], tag) == 0) { StringCchPrintfW(out, on, L"%ls", avail[i]); out[on - 1] = 0; return true; }
    return false;
}

/* Map a preferred language tag (e.g. "de-CH") to one of the available tags; "en" if none fits. */
static bool lang_match(const wchar_t *pref, const wchar_t **avail, int n, wchar_t *out, size_t on)
{
    wchar_t prim[LANG_TAG_MAX];
    size_t pl = 0;
    if (on == 0) return false;
    StringCchCopyW(out, on, L"en");
    if (on < 3) out[0] = 0;
    if (!pref || !pref[0]) return false;
    while (pref[pl] && pref[pl] != L'-' && pl < LANG_TAG_MAX - 1) { prim[pl] = pref[pl]; pl++; }
    prim[pl] = 0;
    if (lang_pick(avail, n, pref, out, on)) return true;
    if (_wcsicmp(prim, L"zh") == 0) {
        bool trad = false;
        for (const wchar_t *p = pref; *p; p++)
            if (_wcsnicmp(p, L"Hant", 4) == 0 || _wcsnicmp(p, L"-TW", 3) == 0 || _wcsnicmp(p, L"-HK", 3) == 0 || _wcsnicmp(p, L"-MO", 3) == 0) trad = true;
        return lang_pick(avail, n, trad ? L"zh-Hant" : L"zh-Hans", out, on);
    }
    if (_wcsicmp(prim, L"pt") == 0) {
        if (lang_pick(avail, n, L"pt-PT", out, on) && _wcsicmp(pref, L"pt") != 0) return true;
        return lang_pick(avail, n, L"pt-BR", out, on);
    }
    if (_wcsicmp(prim, L"sr") == 0) return lang_pick(avail, n, L"sr-Latn", out, on);
    if (_wcsicmp(prim, L"nn") == 0 || _wcsicmp(prim, L"no") == 0) return lang_pick(avail, n, L"nb", out, on);
    for (int i = 0; i < n; i++) {
        size_t k = 0;
        while (avail[i][k] && avail[i][k] != L'-') k++;
        if (k == pl && _wcsnicmp(avail[i], prim, k) == 0) { StringCchPrintfW(out, on, L"%ls", avail[i]); out[on - 1] = 0; return true; }
    }
    return false;
}

/* setting: saved choice ("" = follow Windows). tag receives the language tag to load. */
static void lang_resolve(const wchar_t *setting, wchar_t *tag, size_t n)
{
    LangInfo li[64];
    const wchar_t *av[64];
    int cnt = lang_list(li, 64), na = 0;
    for (int i = 0; i < cnt; i++) if (_wcsicmp(li[i].tag, L"en") != 0) av[na++] = li[i].tag;
    if (n == 0) return;
    if (setting && setting[0]) {
        wchar_t m[LANG_TAG_MAX];
        lang_match(setting, av, na, m, LANG_TAG_MAX);
        StringCchPrintfW(tag, n, L"%ls", m); tag[n - 1] = 0;
        return;
    }
    wchar_t buf[512];
    ULONG num = 0, sz = 512;
    StringCchPrintfW(tag, n, L"en"); tag[n - 1] = 0;
    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &num, buf, &sz)) return;
    for (const wchar_t *p = buf; *p; p += wcslen(p) + 1) {
        if (_wcsicmp(p, L"en") == 0 || _wcsnicmp(p, L"en-", 3) == 0) return;
        wchar_t m[LANG_TAG_MAX];
        if (lang_match(p, av, na, m, LANG_TAG_MAX)) { StringCchPrintfW(tag, n, L"%ls", m); tag[n - 1] = 0; return; }
    }
}
