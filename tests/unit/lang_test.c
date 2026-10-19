/* Unit tests for src/lang.c (string table + language loader). Built by tests\unit\build.bat. */
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <strsafe.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../../src/lang.c"
#include "../../src/locale.c"
#include "../../src/location.c"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static void parse_into(const char *s, size_t n, const wchar_t **t, wchar_t *name, bool *rtl)
{
    for (int i = 0; i < L_COUNT; i++) t[i] = NULL;
    name[0] = 0; *rtl = false;
    lang_parse(s, n, t, name, 48, rtl);
}

static void write_lang_file(const wchar_t *tag, const void *data, size_t n)
{
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    GetModuleFileNameW(NULL, dir, MAX_PATH);
    wchar_t *sl = wcsrchr(dir, L'\\'); if (sl) *sl = 0;
    StringCchCatW(dir, MAX_PATH, L"\\lang");
    CreateDirectoryW(dir, NULL);
    swprintf(path, MAX_PATH, L"%ls\\%ls.txt", dir, tag);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD w;
    if (h != INVALID_HANDLE_VALUE) { WriteFile(h, data, (DWORD)n, &w, NULL); CloseHandle(h); }
}

static void parse_basic(void)
{
    const wchar_t *t[L_COUNT]; wchar_t name[48]; bool rtl;
    const char *s = "@name = Deutsch\naction.title = Aktion\n";
    parse_into(s, strlen(s), t, name, &rtl);
    CHECK(t[L_ACTION_TITLE] && wcscmp(t[L_ACTION_TITLE], L"Aktion") == 0);
    CHECK(wcscmp(name, L"Deutsch") == 0);
    CHECK(!rtl);
    s = "@name = \xD8\xA7\n@rtl = 1\n";
    parse_into(s, strlen(s), t, name, &rtl);
    CHECK(rtl);
}

static void bom_crlf_whitespace(void)
{
    const wchar_t *t[L_COUNT]; wchar_t name[48]; bool rtl;
    const char *s = "\xEF\xBB\xBF" "# comment\r\n  @name  =  Deutsch  \r\n\taction.title\t=   Aktion   \r\n";
    parse_into(s, strlen(s), t, name, &rtl);
    CHECK(t[L_ACTION_TITLE] && wcscmp(t[L_ACTION_TITLE], L"Aktion") == 0);
    CHECK(wcscmp(name, L"Deutsch") == 0);
    CHECK(!rtl);
}

static void escapes(void)
{
    const wchar_t *t[L_COUNT]; wchar_t name[48]; bool rtl;
    const char *s = "action.title = a\\nb\n";
    parse_into(s, strlen(s), t, name, &rtl);
    CHECK(t[L_ACTION_TITLE] && wcscmp(t[L_ACTION_TITLE], L"a\nb") == 0);
    s = "action.title = a\\\\b\n";
    parse_into(s, strlen(s), t, name, &rtl);
    CHECK(t[L_ACTION_TITLE] && wcscmp(t[L_ACTION_TITLE], L"a\\b") == 0);
}

static void missing_key_falls_back(void)
{
    const char *s = "@name = Xx\ntime.minutes = %d Minuten\n";
    write_lang_file(L"xx", s, strlen(s));
    lang_load(L"xx");
    CHECK(wcscmp(TR(L_ACTION_TITLE), L"Action") == 0);
    CHECK(wcscmp(TR(L_TIME_MINUTES), L"%d Minuten") == 0);
    CHECK(wcscmp(lang_tag(), L"xx") == 0);
    lang_load(L"en");
    CHECK(wcscmp(TR(L_TIME_MINUTES), L"%d minutes") == 0);
}

static void placeholder_mismatch_falls_back(void)
{
    const char *s = "rm.now = Jetzt\ntime.minutes = %d Minuten %d\naction.title = Aktion %ls\n";
    write_lang_file(L"xx", s, strlen(s));
    lang_load(L"xx");
    CHECK(wcscmp(TR(L_RM_NOW), L"%ls now") == 0);
    CHECK(wcscmp(TR(L_TIME_MINUTES), L"%d minutes") == 0);
    CHECK(wcscmp(TR(L_ACTION_TITLE), L"Action") == 0);
    s = "rm.now = Jetzt %ls!\n";
    write_lang_file(L"xx", s, strlen(s));
    lang_load(L"xx");
    CHECK(wcscmp(TR(L_RM_NOW), L"Jetzt %ls!") == 0);
    lang_load(L"en");
}

static void placeholder_reorder_falls_back(void)
{
    const char *s = "rm.acc_name = %d Sekunden: %ls\n";
    write_lang_file(L"xx", s, strlen(s));
    lang_load(L"xx");
    CHECK(wcscmp(TR(L_RM_ACC_NAME), L"%ls in %d seconds") == 0);
    s = "rm.acc_name = %ls in %d Sekunden\n";
    write_lang_file(L"xx", s, strlen(s));
    lang_load(L"xx");
    CHECK(wcscmp(TR(L_RM_ACC_NAME), L"%ls in %d Sekunden") == 0);
    lang_load(L"en");
}

static void garbage_file(void)
{
    unsigned char buf[4096]; unsigned x = 12345;
    for (int i = 0; i < (int)sizeof buf; i++) { x = x * 1103515245u + 12345u; buf[i] = (unsigned char)(x >> 16); }
    write_lang_file(L"xx", buf, sizeof buf);
    lang_load(L"xx");
    for (int i = 0; i < L_COUNT; i++) CHECK(TR((StrId)i) != NULL);
    const char *bad = "\xFF\xFE = \x80\naction.title = \xC3\x28\n@name = \xFF\n";
    write_lang_file(L"xx", bad, strlen(bad));
    lang_load(L"xx");
    CHECK(wcscmp(TR(L_ACTION_TITLE), L"Action") == 0);
    lang_load(L"nonexistent");
    CHECK(wcscmp(TR(L_ACTION_TITLE), L"Action") == 0);
    lang_load(L"..\\x");
    CHECK(wcscmp(TR(L_ACTION_TITLE), L"Action") == 0);
    lang_load(L"en");
}

static void resolve_rules(void)
{
    const wchar_t *av[] = { L"de", L"pt-BR", L"pt-PT", L"zh-Hans", L"zh-Hant", L"sr-Latn", L"nb" };
    const struct { const wchar_t *in, *want; } c[] = {
        { L"de-CH", L"de" }, { L"pt-BR", L"pt-BR" }, { L"pt-AO", L"pt-PT" }, { L"zh-CN", L"zh-Hans" },
        { L"zh-HK", L"zh-Hant" }, { L"sr-Cyrl-RS", L"sr-Latn" }, { L"nn-NO", L"nb" }, { L"fa-IR", L"en" },
        { L"de", L"de" }, { L"DE-at", L"de" },
    };
    for (int i = 0; i < (int)(sizeof c / sizeof c[0]); i++) {
        wchar_t out[16];
        lang_match(c[i].in, av, 7, out, 16);
        if (wcscmp(out, c[i].want) != 0) wprintf(L"  %ls -> %ls (want %ls)\n", c[i].in, out, c[i].want);
        CHECK(wcscmp(out, c[i].want) == 0);
    }
    wchar_t tag[16];
    lang_resolve(L"en", tag, 16);
    CHECK(wcscmp(tag, L"en") == 0);
    const char *de = "@name = Deutsch\naction.title = Aktion\n";
    write_lang_file(L"de", de, strlen(de));
    lang_resolve(L"de", tag, 16);
    CHECK(wcscmp(tag, L"de") == 0);
    lang_resolve(L"", tag, 16);
    CHECK(tag[0] != 0);
    /* resolving goes by file name only: a header-less (but correctly named) file still resolves */
    const char *fr = "action.title = Action FR\n";
    write_lang_file(L"fr", fr, strlen(fr));
    lang_resolve(L"fr", tag, 16);
    CHECK(wcscmp(tag, L"fr") == 0);
    g_lang_prefs_test = L"fr-CA\0";                   /* Windows default: by base language */
    lang_resolve(L"", tag, 16);
    CHECK(wcscmp(tag, L"fr") == 0);
    g_lang_prefs_test = L"en-US\0";
    lang_resolve(L"zz", tag, 16);                    /* saved language gone, Windows in English: English */
    CHECK(wcscmp(tag, L"en") == 0);
    lang_resolve(L"fr-CA", tag, 16);                 /* no fr-CA file: like Windows default, not by its base */
    CHECK(wcscmp(tag, L"en") == 0);
    g_lang_prefs_test = NULL;
    lang_resolve(L"EN-gb", tag, 16);
    CHECK(wcscmp(tag, L"en") == 0);
    LangInfo li[64];
    int n = lang_list(li, 64);
    bool listed = false;
    for (int i = 0; i < n; i++) if (wcscmp(li[i].tag, L"fr") == 0) listed = true;
    CHECK(!listed);                                   /* the picker lists only files with a @name */
    /* the picker reads only the header: a big file's @name is found in its first 1 KB */
    static char big[8192];
    StringCchCopyA(big, sizeof big, "@name = Grande\n");
    size_t k = strlen(big);
    while (k < sizeof big - 40) { memcpy(big + k, "action.title = xxxxxxxxxxxxxxxxxxxx\n", 36); k += 36; }
    big[k] = 0;
    write_lang_file(L"gr", big, k);
    n = lang_list(li, 64);
    listed = false;
    for (int i = 0; i < n; i++) if (wcscmp(li[i].tag, L"gr") == 0 && wcscmp(li[i].name, L"Grande") == 0) listed = true;
    CHECK(listed);
    /* no newline in the first 1 KB: the header line that fits is still parsed (not read as empty) */
    StringCchCopyA(big, sizeof big, "@name = Solo");
    k = strlen(big);
    while (k < 1500) big[k++] = ' ';
    memcpy(big + k, "\naction.title = Solo\n", 21); k += 21;
    write_lang_file(L"so", big, k);
    n = lang_list(li, 64);
    listed = false;
    for (int i = 0; i < n; i++) if (wcscmp(li[i].tag, L"so") == 0 && wcscmp(li[i].name, L"Solo") == 0) listed = true;
    CHECK(listed);
    wchar_t dir[MAX_PATH], p[MAX_PATH];
    lang_dir(dir, MAX_PATH);
    swprintf(p, MAX_PATH, L"%ls\\fr.txt", dir); DeleteFileW(p);
    swprintf(p, MAX_PATH, L"%ls\\gr.txt", dir); DeleteFileW(p);
    swprintf(p, MAX_PATH, L"%ls\\so.txt", dir); DeleteFileW(p);
}

/* a saved language whose file is gone resolves exactly like Windows default (preferred UI languages:
   exact, then base, then English), and the title bar's Language button reads that language's own name */
static void saved_missing_follows_windows(void)
{
    wchar_t tag[16], c[128];
    const char *de = "@name = Deutsch\naction.title = Aktion\n";
    write_lang_file(L"de", de, strlen(de));
    g_lang_prefs_test = L"de-DE\0en-US\0";
    lang_resolve(L"zz", tag, 16);
    CHECK(wcscmp(tag, L"de") == 0);
    lang_load(tag);
    lang_caption(L"zz", c, 128);
    CHECK(wcscmp(c, L"Deutsch") == 0);
    lang_resolve(L"", tag, 16);                       /* the same as following Windows */
    CHECK(wcscmp(tag, L"de") == 0);
    g_lang_prefs_test = L"xh-ZA\0de-AT\0";              /* first preferred has no file: the next one, by base */
    lang_resolve(L"zz", tag, 16);
    CHECK(wcscmp(tag, L"de") == 0);
    g_lang_prefs_test = L"en-GB\0de-DE\0";              /* English first: English */
    lang_resolve(L"zz", tag, 16);
    CHECK(wcscmp(tag, L"en") == 0);
    g_lang_prefs_test = L"xh-ZA\0";                     /* nothing fits: English */
    lang_resolve(L"zz", tag, 16);
    CHECK(wcscmp(tag, L"en") == 0);
    lang_load(L"en");
    lang_caption(L"zz", c, 128);
    CHECK(wcscmp(c, L"English") == 0);
    g_lang_prefs_test = NULL;
}

/* the title bar's Language button: the loaded language's own name (no folder listing), whether it was
   chosen or followed from Windows -- never "Windows default (...)", which only the dropdown's first entry says */
static void picker_caption(void)
{
    wchar_t c[128];
    lang_load(L"en");
    lang_caption(L"", c, 128);
    CHECK(wcscmp(c, L"English") == 0);
    lang_caption(L"en", c, 128);
    CHECK(wcscmp(c, L"English") == 0);
    lang_caption(L"qps-ploc", c, 128);               /* saved file gone, English loaded */
    CHECK(wcscmp(c, L"English") == 0);
    const char *x = "@name = Xx-Sprache\nlanguage.default = Standard (%ls)\n";
    write_lang_file(L"xx", x, strlen(x));
    lang_load(L"xx");
    CHECK(wcscmp(lang_name(), L"Xx-Sprache") == 0);
    lang_caption(L"xx", c, 128);
    CHECK(wcscmp(c, L"Xx-Sprache") == 0);
    lang_caption(L"", c, 128);                       /* following Windows, which resolved to xx */
    CHECK(wcscmp(c, L"Xx-Sprache") == 0);
    const char *y = "action.title = Y\n";              /* no @name: the tag stands in */
    write_lang_file(L"yy", y, strlen(y));
    lang_load(L"yy");
    CHECK(wcscmp(lang_name(), L"yy") == 0);
    lang_load(L"en");
    CHECK(wcscmp(lang_name(), L"English") == 0);
    wchar_t dir[MAX_PATH], p[MAX_PATH];
    lang_dir(dir, MAX_PATH);
    swprintf(p, MAX_PATH, L"%ls\\yy.txt", dir); DeleteFileW(p);
}

static void every_key_unique(void)
{
    for (int i = 0; i < L_COUNT; i++)
        for (int j = i + 1; j < L_COUNT; j++)
            if (strcmp(g_lang_keys[i], g_lang_keys[j]) == 0) {
                printf("  duplicate key %s\n", g_lang_keys[i]);
                CHECK(!"duplicate key");
            }
}

/* every English string uses only the placeholders a translation can be checked against */
static void english_placeholders_valid(void)
{
    for (int i = 0; i < L_COUNT; i++) {
        char seq[32];
        if (!lang_ph_seq(g_lang_en[i], seq, 32)) {
            printf("  bad placeholder in %s\n", g_lang_keys[i]);
            CHECK(!"bad placeholder");
        }
    }
}

/* lang\en.txt (tools\gen-lang-template.ps1) parses back to exactly the built-in English */
static void en_template_matches(void)
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t *sl = wcsrchr(path, L'\\'); if (sl) *sl = 0;
    StringCchCatW(path, MAX_PATH, L"\\..\\..\\..\\lang\\en.txt");
    size_t len = 0;
    char *data = lang_read_file(path, &len);
    CHECK(data != NULL);
    if (!data) return;
    const wchar_t *t[L_COUNT]; wchar_t name[48]; bool rtl;
    parse_into(data, len, t, name, &rtl);
    HeapFree(GetProcessHeap(), 0, data);
    CHECK(wcscmp(name, L"English") == 0);
    for (int i = 0; i < L_COUNT; i++)
        if (!t[i] || wcscmp(t[i], g_lang_en[i]) != 0) {
            printf("  en.txt differs at %s\n", g_lang_keys[i]);
            CHECK(!"en.txt matches strings.h");
        }
}

/* src/locale.c: regional clock, localized day and AM/PM names */
static void clock_and_day_names(void)
{
    wchar_t t[32];
    g_clock_test = L"HH:mm";                            /* 24-hour */
    fmt_time(23, 5, t, 32); CHECK(wcscmp(t, L"23:05") == 0);
    fmt_time(9, 5, t, 32); CHECK(wcscmp(t, L"09:05") == 0);
    CHECK(clock_24h());
    g_clock_test = L"H.mm";
    fmt_time(9, 5, t, 32); CHECK(wcscmp(t, L"9:05") == 0);
    g_clock_test = L"h:mm tt";                          /* 12-hour */
    CHECK(!clock_24h());
    lang_load(L"en");
    fmt_time(23, 5, t, 32); CHECK(wcscmp(t, L"11:05\u00A0PM") == 0);
    /* the regional format's order and padding */
    fmt_time_with(L"tt h:mm", 23, 5, t, 32); CHECK(wcscmp(t, L"PM\u00A011:05") == 0);
    fmt_time_with(L"h:mm tt", 23, 5, t, 32); CHECK(wcscmp(t, L"11:05\u00A0PM") == 0);
    fmt_time_with(L"hh:mm tt", 9, 5, t, 32); CHECK(wcscmp(t, L"09:05\u00A0AM") == 0);
    fmt_time_with(L"'tt' h:mm tt", 23, 5, t, 32); CHECK(wcscmp(t, L"11:05\u00A0PM") == 0);  /* quoted text ignored */
    fmt_time_with(L"tt HH:mm", 23, 5, t, 32); CHECK(wcscmp(t, L"23:05") == 0);         /* 24-hour: no marker */
    fmt_time(0, 0, t, 32); CHECK(wcscmp(t, L"12:00\u00A0AM") == 0);
    fmt_time(12, 30, t, 32); CHECK(wcscmp(t, L"12:30\u00A0PM") == 0);
    day_name(0, false, t, 32); CHECK(wcscmp(t, L"Mon") == 0);
    day_name(6, true, t, 32); CHECK(wcscmp(t, L"Sunday") == 0);
    ampm(true, t, 32); CHECK(wcscmp(t, L"PM") == 0);
    const char *de = "@name = Deutsch\naction.title = Aktion\n";
    write_lang_file(L"de", de, strlen(de));
    lang_load(L"de");
    CHECK(wcscmp(lang_tag(), L"de") == 0);
    day_name(0, false, t, 32); CHECK(wcscmp(t, L"Mo") == 0);
    day_name(6, true, t, 32); CHECK(wcscmp(t, L"Sonntag") == 0);
    ampm(false, t, 32); CHECK(t[0] != 0);
    day_name(7, true, t, 32); CHECK(t[0] == 0);         /* out of range: empty, no crash */
    /* right-to-left UI: a Latin AM/PM marker keeps its place after the time (left-to-right
       embedding); one in an RTL script reads with the sentence as is */
    const char *xr = "@name = Xx\n@rtl = 1\n";
    write_lang_file(L"xx", xr, strlen(xr));
    lang_load(L"xx");                                   /* no Windows locale: English markers */
    CHECK(lang_rtl());
    fmt_time_with(L"h:mm tt", 23, 5, t, 32); CHECK(wcscmp(t, L"\u202A" L"11:05\u00A0PM\u202C") == 0);
    fmt_time_with(L"HH:mm", 23, 5, t, 32); CHECK(wcscmp(t, L"23:05") == 0);
    write_lang_file(L"ar", xr, strlen(xr));
    lang_load(L"ar");
    fmt_time_with(L"h:mm tt", 23, 5, t, 32); CHECK(t[0] == L'1' && has_rtl_letters(t));
    CHECK(has_rtl_letters(L"\u05D0"));                  /* Hebrew */
    CHECK(has_rtl_letters(L"x\u0645"));                 /* Arabic */
    CHECK(!has_rtl_letters(L"PM"));
    CHECK(!has_rtl_letters(L""));
    lang_load(L"en");
    g_clock_test = NULL;                                /* follows Windows */
    int f = first_day();
    CHECK(f >= 0 && f <= 6);
    fmt_time(23, 5, t, 32); CHECK(t[0] != 0);
}

/* src/lang.c: CJK languages draw with Windows' own UI face for their script (Segoe UI's fallback
   misses kana and hanzi); every other language keeps Segoe UI Variable */
static void ui_font_faces(void)
{
    CHECK(wcscmp(ui_font_face(L"ja"), L"Yu Gothic UI") == 0);
    CHECK(wcscmp(ui_font_face(L"zh-Hans"), L"Microsoft YaHei UI") == 0);
    CHECK(wcscmp(ui_font_face(L"zh-Hant"), L"Microsoft JhengHei UI") == 0);
    CHECK(wcscmp(ui_font_face(L"ko"), L"Malgun Gothic") == 0);
    CHECK(wcscmp(ui_font_face(L"ZH-HANT"), L"Microsoft JhengHei UI") == 0);  /* tags ignore case */
    CHECK(wcscmp(ui_font_face(L"en"), L"Segoe UI Variable Text") == 0);
    CHECK(wcscmp(ui_font_face(L"ru"), L"Segoe UI Variable Text") == 0);
    CHECK(wcscmp(ui_font_face(L"jav"), L"Segoe UI Variable Text") == 0);    /* not a prefix match */
    CHECK(wcscmp(ui_font_face(L""), L"Segoe UI Variable Text") == 0);
    CHECK(wcscmp(ui_font_face(NULL), L"Segoe UI Variable Text") == 0);
}

/* tests\sandbox\qps-ploc.txt (gen-lang-template.ps1 -Pseudo): every key loads (placeholders kept) */
static void pseudo_file_complete(void)
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t *sl = wcsrchr(path, L'\\'); if (sl) *sl = 0;
    StringCchCatW(path, MAX_PATH, L"\\..\\..\\..\\tests\\sandbox\\qps-ploc.txt");
    size_t len = 0;
    char *data = lang_read_file(path, &len);
    CHECK(data != NULL);
    if (!data) return;
    const wchar_t *t[L_COUNT]; wchar_t name[48]; bool rtl;
    parse_into(data, len, t, name, &rtl);
    HeapFree(GetProcessHeap(), 0, data);
    CHECK(wcscmp(name, L"Pseudo") == 0);
    for (int i = 0; i < L_COUNT; i++)
        if (!t[i] || wcslen(t[i]) < wcslen(g_lang_en[i]) * 14 / 10) {
            printf("  qps-ploc.txt missing or short at %s\n", g_lang_keys[i]);
            CHECK(!"pseudo value loads and is 40 percent longer");
        }
}

/* the language picker's entries: Windows default (named after what it resolves to), then each language */
static void picker_items(void)
{
    LangInfo li[3] = { { L"en", L"English", false }, { L"de", L"Deutsch", false }, { L"qps-ploc", L"Pseudo", false } };
    LangItem it[8];
    lang_load(L"en");
    int n = lang_items(li, 3, L"de", it, 8);
    CHECK(n == 4);
    CHECK(it[0].tag[0] == 0 && wcscmp(it[0].label, L"Windows default (Deutsch)") == 0);
    CHECK(wcscmp(it[1].tag, L"en") == 0 && wcscmp(it[1].label, L"English") == 0);
    CHECK(wcscmp(it[3].tag, L"qps-ploc") == 0 && wcscmp(it[3].label, L"Pseudo") == 0);
    n = lang_items(li, 3, L"xx", it, 8);              /* resolved tag not listed: named English */
    CHECK(wcscmp(it[0].label, L"Windows default (English)") == 0);
    n = lang_items(li, 1, L"en", it, 8);              /* no lang folder: Windows default and English */
    CHECK(n == 2);
    CHECK(lang_items(li, 3, L"en", it, 2) == 2);      /* capped at max */
    n = lang_items(li, 3, L"en", it, 8);
    CHECK(lang_item_index(it, n, L"") == 0);
    CHECK(lang_item_index(it, n, L"en") == 1);
    CHECK(lang_item_index(it, n, L"QPS-PLOC") == 3);
    CHECK(lang_item_index(it, n, L"fr") == 0);        /* saved language missing: Windows default */
    CHECK(lang_item_index(it, n, NULL) == 0);
    const char *x = "@name = Xx\nlanguage.default = Standard (%ls)\n";
    write_lang_file(L"xx", x, strlen(x));
    lang_load(L"xx");                                 /* the label is in the current language */
    lang_items(li, 3, L"de", it, 8);
    CHECK(wcscmp(it[0].label, L"Standard (Deutsch)") == 0);
    lang_load(L"en");
}

/* the Language dropdown draws each entry in a face that has its own name's script: CJK names in
   their script's UI face (in any UI language: Arabic's Segoe UI drew them as boxes), the rest in the
   UI's face; "Windows default (<name>)" by the name it carries */
static void picker_item_faces(void)
{
    LangItem it[6] = { { L"", L"Windows default (English)" }, { L"ja", L"x" }, { L"zh-Hans", L"x" },
                       { L"zh-Hant", L"x" }, { L"ko", L"x" }, { L"de", L"Deutsch" } };
    const wchar_t *SEGOE = L"Segoe UI Variable Text";
    CHECK(wcscmp(lang_item_face(&it[1], L"en", L"ar"), L"Yu Gothic UI") == 0);
    CHECK(wcscmp(lang_item_face(&it[2], L"en", L"ar"), L"Microsoft YaHei UI") == 0);
    CHECK(wcscmp(lang_item_face(&it[3], L"en", L"ar"), L"Microsoft JhengHei UI") == 0);
    CHECK(wcscmp(lang_item_face(&it[4], L"en", L"ar"), L"Malgun Gothic") == 0);
    CHECK(wcscmp(lang_item_face(&it[5], L"en", L"ar"), SEGOE) == 0);
    CHECK(wcscmp(lang_item_face(&it[0], L"en", L"ar"), SEGOE) == 0);
    CHECK(wcscmp(lang_item_face(&it[0], L"ja", L"ar"), L"Yu Gothic UI") == 0);   /* "... (日本語)" */
    CHECK(wcscmp(lang_item_face(&it[0], L"de", L"ja"), L"Yu Gothic UI") == 0);   /* Japanese label */
    CHECK(wcscmp(lang_item_face(&it[5], L"en", L"ja"), L"Yu Gothic UI") == 0);   /* the UI's face */
    CHECK(wcscmp(lang_item_face(&it[4], L"en", L"ja"), L"Malgun Gothic") == 0);
    CHECK(wcscmp(lang_item_face(&it[1], L"en", L"ja"), L"Yu Gothic UI") == 0);
    CHECK(wcscmp(lang_item_face(&it[1], L"en", L"en"), L"Yu Gothic UI") == 0);
    CHECK(wcscmp(lang_item_face(&it[5], NULL, NULL), SEGOE) == 0);
}

static void installed_location(void)
{
    const wchar_t *la = L"C:\\Users\\Ann\\AppData\\Local";
    CHECK(is_installed_location(L"C:\\Users\\Ann\\AppData\\Local\\Programs\\PowerNapp\\powernapp.exe", la));
    CHECK(is_installed_location(L"c:\\users\\ann\\appdata\\local\\programs\\POWERNAPP\\PowerNapp.exe", la));
    CHECK(is_installed_location(L"C:\\Users\\Ann\\AppData\\Local\\Programs\\PowerNapp\\powernapp.exe", L"C:\\Users\\Ann\\AppData\\Local\\"));
    CHECK(is_installed_location(L"C:\\Users\\Ann\\AppData\\Local\\Programs\\PowerNapp\\powernapp.exe", L"C:\\Users\\Ann\\AppData\\Local\\\\"));
    CHECK(!is_installed_location(L"C:\\Temp\\x\\powernapp.exe", la));
    CHECK(!is_installed_location(L"C:\\Users\\Ann\\AppData\\Local\\Programs\\PowerNappX\\powernapp.exe", la));
    CHECK(!is_installed_location(L"C:\\Users\\Ann\\AppData\\Local\\Programs\\PowerNapp\\sub\\powernapp.exe", la));
    CHECK(!is_installed_location(L"C:\\Users\\Ann\\AppData\\Local\\Programs\\PowerNapp", la)); /* no file part */
    CHECK(!is_installed_location(L"C:\\Users\\Ann\\AppData\\Local\\Programs\\PowerNapp\\powernapp.exe", L""));
    CHECK(!is_installed_location(L"C:\\Users\\Ann\\AppData\\Local\\Programs\\PowerNapp\\powernapp.exe", NULL));
    CHECK(!is_installed_location(NULL, la));
}

int main(void)
{
    installed_location();
    every_key_unique();
    en_template_matches();
    english_placeholders_valid();
    parse_basic();
    bom_crlf_whitespace();
    escapes();
    missing_key_falls_back();
    placeholder_mismatch_falls_back();
    placeholder_reorder_falls_back();
    garbage_file();
    resolve_rules();
    clock_and_day_names();
    ui_font_faces();
    picker_items();
    picker_item_faces();
    picker_caption();
    saved_missing_follows_windows();
    pseudo_file_complete();
    printf(g_fail ? "%d check(s) FAILED\n" : "all checks passed\n", g_fail);
    return g_fail;
}
