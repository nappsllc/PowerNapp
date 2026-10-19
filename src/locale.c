/* locale.c - dates and times.  The clock (12/24-hour) and the first day of the week follow the
   Windows regional format, in every language; day and AM/PM names come from Windows' locale data
   for the chosen UI language (lang_tag()).  Days are Monday-based everywhere: 0 = Monday ... 6 = Sunday. */

static const wchar_t *g_clock_test;   /* tests: a time format to use instead of the user's ("HH:mm", "tt h:mm") */

/* What the clock looks like, read from a Windows time format string (LOCALE_STIMEFORMAT syntax):
   H/HH = 24-hour, h/hh = 12-hour, doubled = leading zero, t/tt before the hour = AM/PM first. */
typedef struct { bool h24, pad, ap_first; } ClockFmt;
static ClockFmt clock_parse(const wchar_t *f)
{
    ClockFmt c = { false, false, false };
    bool quoted = false, ap = false;
    for (const wchar_t *p = f ? f : L""; *p; p++) {
        if (*p == L'\'') quoted = !quoted;
        else if (quoted) continue;
        else if (*p == L't') ap = true;
        else if (*p == L'H' || *p == L'h') {
            c.h24 = *p == L'H';
            c.pad = p[1] == *p;
            c.ap_first = ap && !c.h24;
            break;
        }
    }
    return c;
}
/* the user's regional time format */
static void clock_fmt_str(wchar_t *f, int n)
{
    if (g_clock_test) StringCchCopyW(f, n, g_clock_test);
    else if (!GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT, f, n)) f[0] = 0;
}
static ClockFmt clock_fmt(void)
{
    wchar_t f[80];
    clock_fmt_str(f, 80);
    return clock_parse(f);
}
static bool clock_24h(void) { return clock_fmt().h24; }

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

static void fmt_hour_c(ClockFmt c, int h, wchar_t *out, size_t n)
{
    if (!c.h24) h = h % 12 ? h % 12 : 12;
    StringCchPrintfW(out, n, c.pad ? L"%02d" : L"%d", h);
}
/* hour as shown in the time button / picker: "23", "09"/"9", or "11" (12-hour) */
static void fmt_hour(int h, wchar_t *out, size_t n) { fmt_hour_c(clock_fmt(), h, out, n); }

/* any Hebrew or Arabic-script letter (strong right-to-left) */
static bool has_rtl_letters(const wchar_t *s)
{
    for (; s && *s; s++)
        if ((*s >= 0x0590 && *s <= 0x08FF) || (*s >= 0xFB1D && *s <= 0xFDFF) || (*s >= 0xFE70 && *s <= 0xFEFC))
            return true;
    return false;
}
/* time in the order of a Windows time format: "23:05", "11:05 PM", "오후 11:05", "09:05 AM"
   (a no-break space between the time and the marker) */
static void fmt_time_with(const wchar_t *fmt, int h, int m, wchar_t *out, size_t n)
{
    ClockFmt c = clock_parse(fmt);
    wchar_t hh[8], ap[16];
    fmt_hour_c(c, h, hh, 8);
    if (c.h24) { StringCchPrintfW(out, n, L"%ls:%02d", hh, m); return; }
    ampm(h >= 12, ap, 16);
    /* no-break space: a wrapped line never leaves the AM/PM marker on its own */
    if (c.ap_first) StringCchPrintfW(out, n, L"%ls\u00A0%ls:%02d", ap, hh, m);
    else StringCchPrintfW(out, n, L"%ls:%02d\u00A0%ls", hh, m, ap);
    /* right-to-left UI: a Latin marker would land before the time ("PM 11:00"), so the time reads
       as one left-to-right embedding; a marker in an RTL script already reads right with the sentence */
    if (lang_rtl() && !has_rtl_letters(ap)) {
        wchar_t tmp[64];
        StringCchCopyW(tmp, 64, out);
        StringCchPrintfW(out, n, L"%lc%ls%lc", (wchar_t)0x202A, tmp, (wchar_t)0x202C); /* LRE ... PDF */
    }
}
/* with the user's regional clock */
static void fmt_time(int h, int m, wchar_t *out, size_t n)
{
    wchar_t f[80];
    clock_fmt_str(f, 80);
    fmt_time_with(f, h, m, out, n);
}
