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
    fmt_time(23, 5, t, 32); CHECK(wcscmp(t, L"11:05 PM") == 0);
    /* the regional format's order and padding */
    fmt_time_with(L"tt h:mm", 23, 5, t, 32); CHECK(wcscmp(t, L"PM 11:05") == 0);
    fmt_time_with(L"h:mm tt", 23, 5, t, 32); CHECK(wcscmp(t, L"11:05 PM") == 0);
    fmt_time_with(L"hh:mm tt", 9, 5, t, 32); CHECK(wcscmp(t, L"09:05 AM") == 0);
    fmt_time_with(L"'tt' h:mm tt", 23, 5, t, 32); CHECK(wcscmp(t, L"11:05 PM") == 0);  /* quoted text ignored */
    fmt_time_with(L"tt HH:mm", 23, 5, t, 32); CHECK(wcscmp(t, L"23:05") == 0);         /* 24-hour: no marker */
    fmt_time(0, 0, t, 32); CHECK(wcscmp(t, L"12:00 AM") == 0);
    fmt_time(12, 30, t, 32); CHECK(wcscmp(t, L"12:30 PM") == 0);
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
    lang_load(L"en");
    g_clock_test = NULL;                                /* follows Windows */
    int f = first_day();
    CHECK(f >= 0 && f <= 6);
    fmt_time(23, 5, t, 32); CHECK(t[0] != 0);
}

int main(void)
{
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
    printf(g_fail ? "%d check(s) FAILED\n" : "all checks passed\n", g_fail);
    return g_fail;
}
