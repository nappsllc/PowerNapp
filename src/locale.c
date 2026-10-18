/* locale.c - dates and times.  The clock (12/24-hour) and the first day of the week follow the
   Windows regional format, in every language; day and AM/PM names come from Windows' locale data
   for the chosen UI language (lang_tag()).  Days are Monday-based everywhere: 0 = Monday ... 6 = Sunday. */

static int g_clock_test = -1;   /* tests: -1 follow Windows, 0 12-hour, 1 24-hour "H:mm", 2 24-hour "HH:mm" */

/* 0 = 12-hour, 1 = 24-hour, 2 = 24-hour with a leading zero (from the user's LOCALE_STIMEFORMAT) */
static int clock_kind(void)
{
    if (g_clock_test >= 0) return g_clock_test;
    wchar_t f[80];
    if (!GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT, f, 80)) return 0;
    bool quoted = false;
    for (const wchar_t *p = f; *p; p++) {
        if (*p == L'\'') quoted = !quoted;
        else if (!quoted && *p == L'H') return p[1] == L'H' ? 2 : 1;
    }
    return 0;
}
static bool clock_24h(void) { return clock_kind() != 0; }

/* 0 = Monday ... 6 = Sunday (LOCALE_IFIRSTDAYOFWEEK uses the same numbering) */
static int first_day(void)
{
    wchar_t v[4];
    if (!GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_IFIRSTDAYOFWEEK, v, 4)) return 0;
    return v[0] >= L'0' && v[0] <= L'6' ? v[0] - L'0' : 0;
}

/* Windows' value for the UI language; English if the language has none */
static void locale_str(LCTYPE t, const wchar_t *en, wchar_t *out, size_t n)
{
    if (!n) return;
    if (GetLocaleInfoEx(lang_tag(), t, out, (int)n) <= 0 || !out[0]) StringCchCopyW(out, n, en);
}

static void day_name(int d, bool full, wchar_t *out, size_t n)
{
    static const wchar_t *const EN[2][7] = {
        { L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat", L"Sun" },
        { L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday", L"Sunday" },
    };
    if (!n) return;
    out[0] = 0;
    if (d < 0 || d > 6) return;
    locale_str((full ? LOCALE_SDAYNAME1 : LOCALE_SABBREVDAYNAME1) + d, EN[full][d], out, n);
}

static void ampm(bool pm, wchar_t *out, size_t n)
{
    locale_str(pm ? LOCALE_SPM : LOCALE_SAM, pm ? L"PM" : L"AM", out, n);
}

/* hour as shown in the time button / picker: "23", "09"/"9", or "11" (12-hour) */
static void fmt_hour(int h, wchar_t *out, size_t n)
{
    int k = clock_kind();
    if (k == 0) StringCchPrintfW(out, n, L"%d", h % 12 ? h % 12 : 12);
    else StringCchPrintfW(out, n, k == 2 ? L"%02d" : L"%d", h);
}

/* "23:05" or "11:05 PM" */
static void fmt_time(int h, int m, wchar_t *out, size_t n)
{
    wchar_t hh[8], ap[16];
    fmt_hour(h, hh, 8);
    if (clock_24h()) { StringCchPrintfW(out, n, L"%ls:%02d", hh, m); return; }
    ampm(h >= 12, ap, 16);
    StringCchPrintfW(out, n, L"%ls:%02d %ls", hh, m, ap);
}
