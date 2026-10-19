/* Unit tests for the app's own logic (command line, migration, layout invariants): the whole unity
   build with its entry point renamed. Built by tests\unit\build.bat. Calls nothing that touches
   Task Scheduler, the registry or the INI: the migration test runs on fake Task Scheduler calls. */
#define wmain powernapp_wmain
#include "../../powernapp.c"
#undef wmain

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

/* --task spellings: the names, PowerOff 1.x's aliases ("poweroff" = shut down), not the app's name */
static void task_aliases(void)
{
    Task t = T_LOCK;
    CHECK(task_from_arg(L"shutdown", &t) && t == T_SHUTDOWN);
    CHECK(task_from_arg(L"poweroff", &t) && t == T_SHUTDOWN);
    CHECK(task_from_arg(L"Power-Off", &t) && t == T_SHUTDOWN);
    CHECK(task_from_arg(L"reboot", &t) && t == T_RESTART);
    CHECK(task_from_arg(L"sign_out", &t) && t == T_LOGOFF);
    CHECK(task_from_arg(L"monitor-off", &t) && t == T_DISPLAY);
    CHECK(task_from_arg(L"suspend", &t) && t == T_SLEEP);
    CHECK(!task_from_arg(L"powernapp", &t));
    CHECK(!task_from_arg(L"bogus", &t));
}

/* ---- migration from PowerOff 1.x on fake Task Scheduler calls ---- */
static const wchar_t *g_fake_have[4];   /* old tasks that exist */
static const wchar_t *g_fake_fail;      /* the new name whose registration fails */
static wchar_t g_fake_deleted[4][32], g_fake_registered[4][32];
static int g_fake_ndel, g_fake_nreg;
static bool g_fake_nocmd;               /* old tasks have no <Command> */
static bool fake_read(const wchar_t *name, TaskInfo *ti)
{
    for (int i = 0; i < 4; i++)
        if (g_fake_have[i] && !wcscmp(g_fake_have[i], name)) {
            StringCchCopyW(ti->xml, 8192, g_fake_nocmd
                ? L"<Task><Actions><ComHandler><ClassId>{00000000-0000-0000-0000-000000000000}</ClassId></ComHandler></Actions></Task>"
                : L"<Task><Actions><Exec><Command>C:\\old\\poweroff.exe</Command>"
                  L"<Arguments>--wake</Arguments></Exec></Actions></Task>");
            return true;
        }
    return false;
}
static bool fake_reg(const wchar_t *name, const wchar_t *xml)
{
    if (g_fake_fail && !wcscmp(name, g_fake_fail)) return false;
    if (!wcsstr(xml, L"<Command>C:\\new\\powernapp.exe</Command>")) return false;
    StringCchCopyW(g_fake_registered[g_fake_nreg++], 32, name);
    return true;
}
static void fake_del(const wchar_t *name) { StringCchCopyW(g_fake_deleted[g_fake_ndel++], 32, name); }
static bool fake_deleted(const wchar_t *name)
{
    for (int i = 0; i < g_fake_ndel; i++) if (!wcscmp(g_fake_deleted[i], name)) return true;
    return false;
}
static bool fake_run(const wchar_t *a, const wchar_t *b, const wchar_t *fail)
{
    static const TsCalls fake = { fake_read, fake_reg, fake_del };
    g_fake_have[0] = a; g_fake_have[1] = b; g_fake_have[2] = g_fake_have[3] = NULL;
    g_fake_fail = fail; g_fake_ndel = g_fake_nreg = 0;
    return migrate_tasks(&fake, L"C:\\new\\powernapp.exe");
}

static void migration_keeps_unmoved_tasks(void)
{
    /* every task moves: success, the old ones are gone */
    CHECK(fake_run(L"PowerOff", L"PowerOff wake", NULL));
    CHECK(g_fake_nreg == 2 && fake_deleted(L"PowerOff") && fake_deleted(L"PowerOff wake"));
    /* nothing to move: success */
    CHECK(fake_run(NULL, NULL, NULL));
    CHECK(g_fake_nreg == 0 && g_fake_ndel == 0);
    /* the wake task cannot be registered: failure, and its old task is kept */
    CHECK(!fake_run(L"PowerOff", L"PowerOff wake", WTS_WAKE));
    CHECK(fake_deleted(L"PowerOff") && !fake_deleted(L"PowerOff wake"));
    /* an old task with no command to point at the new exe (edited by hand): failure, kept */
    g_fake_nocmd = true;
    CHECK(!fake_run(L"PowerOff daily", NULL, NULL));
    CHECK(g_fake_nreg == 0 && !fake_deleted(L"PowerOff daily"));
    g_fake_nocmd = false;
}

/* Korean wrapped text (the About description here) breaks at spaces, never inside a Hangul word */
static int lines_of(HDC dc, const wchar_t *s, int wrap)
{
    RECT one = { 0, 0, wrap, 0 }, all = { 0, 0, wrap, 0 };
    DrawTextW(dc, L"X", -1, &one, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    DrawTextW(dc, s, -1, &all, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    return one.bottom ? all.bottom / one.bottom : 0;
}
static void korean_wraps_at_spaces(void)
{
    const wchar_t *ko = L"일정에 따라 PC를 종료하거나 다시 "
        L"시작하거나 절전 상태로 두는 작은 앱입니다. "
        L"Windows 작업 스케줄러가 작업을 처리하므로 앱을 "
        L"닫으면 메모리를 전혀 사용하지 않습니다.";
    HDC dc = CreateCompatibleDC(NULL);
    StringCchCopyW(g_lang_tag, LANG_TAG_MAX, L"ko");
    for (int px = 14; px <= 21; px += 7) {               /* the About font at 96 and 144 DPI */
        HFONT f = CreateFontW(-px, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Malgun Gothic");
        HGDIOBJ o = SelectObject(dc, f);
        for (int wrap = 300 * px / 14; wrap <= 400 * px / 14; wrap += 13) {
            const wchar_t *d = wrap_words(dc, ko, wrap);
            bool same = wcslen(d) == wcslen(ko);
            int nl = 0;
            for (size_t i = 0; same && ko[i]; i++) {
                if (d[i] == L'\n' && ko[i] == L' ') nl++;
                else if (d[i] != ko[i]) same = false;
            }
            CHECK(same);                                  /* only spaces became line breaks */
            int drawn = lines_of(dc, d, wrap);
            if (drawn != nl + 1) printf("  %d px, wrap %d: GDI draws %d lines, %d broken at spaces\n", px, wrap, drawn, nl + 1);
            CHECK(drawn == nl + 1);                       /* GDI adds no break of its own */
        }
        SelectObject(dc, o);
        DeleteObject(f);
    }
    StringCchCopyW(g_lang_tag, LANG_TAG_MAX, L"ja");    /* other languages: as GDI breaks them */
    CHECK(wrap_words(dc, ko, 352) == ko);
    StringCchCopyW(g_lang_tag, LANG_TAG_MAX, L"en");
    DeleteDC(dc);
}

/* the footer's Language picker, link style: the 16 px globe, 6 px gap, the name; never wider than the
   room the footer leaves beside About (the name then ellipsizes, and --check-fit reports it) */
static void foot_lang_width(void)
{
    int dpi = g_dpi;
    bool fits = false;
    g_dpi = 96;
    CHECK(foot_lang_w(50, 300, &fits) == 72 && fits);
    CHECK(foot_lang_w(278, 300, &fits) == 300 && fits);        /* exactly the room */
    CHECK(foot_lang_w(279, 300, &fits) == 300 && !fits);       /* one px more: capped, reported */
    CHECK(foot_lang_w(400, 10, &fits) == 16 && !fits);         /* no room at all: still the globe */
    CHECK(foot_lang_w(50, 300, NULL) == 72);
    g_dpi = 144;
    CHECK(foot_lang_w(75, 450, &fits) == 75 + 33 && fits);     /* globe and gap scale, text is device px */
    CHECK(foot_lang_w(500, 450, &fits) == 450 && !fits);
    g_dpi = dpi;
}

/* the Language list opens above the footer picker, 4 px over it, left-aligned with it (right-aligned in
   right-to-left), inside the work area; fewer rows (then it scrolls) when the space above is short */
static void fly_above_placement(void)
{
    int dpi = g_dpi, rows = 0;
    RECT work = { 0, 0, 1920, 1040 }, sr = { 500, 700, 580, 720 };
    POINT pt;
    g_dpi = 96;
    pt = fly_above(&sr, 208, 37, &work, false, &rows);
    CHECK(rows == 12 && pt.x == 500 && pt.y == 700 - 4 - (12 * 36 + 4));
    pt = fly_above(&sr, 208, 37, &work, true, &rows);          /* right-to-left: trailing edges line up */
    CHECK(rows == 12 && pt.x == 580 - 208 && pt.y == 260);
    pt = fly_above(&sr, 208, 3, &work, false, &rows);          /* a short list: just its rows */
    CHECK(rows == 3 && pt.y == 700 - 4 - (3 * 36 + 4));
    RECT hi = { 500, 300, 580, 320 };                          /* little room above: 8 rows, still inside */
    pt = fly_above(&hi, 208, 37, &work, false, &rows);
    CHECK(rows == 8 && pt.y == 300 - 4 - (8 * 36 + 4) && pt.y >= work.top);
    RECT wk2 = { 0, 100, 1920, 1040 }, hi2 = { 500, 400, 580, 420 }; /* work area not at 0 (top taskbar) */
    pt = fly_above(&hi2, 208, 37, &wk2, false, &rows);
    CHECK(rows == 8 && pt.y == 104);
    RECT top = { 500, 20, 580, 40 };                           /* no room at all: clamped to the top */
    pt = fly_above(&top, 208, 37, &work, false, &rows);
    CHECK(pt.y == work.top);
    RECT rgt = { 1800, 700, 1880, 720 }, lft = { 20, 700, 100, 720 };
    pt = fly_above(&rgt, 208, 37, &work, false, &rows);        /* kept inside on the right */
    CHECK(pt.x == 1920 - 208);
    pt = fly_above(&lft, 208, 37, &work, true, &rows);         /* and on the left */
    CHECK(pt.x == 0);
    g_dpi = 144;
    pt = fly_above(&sr, 312, 37, &work, false, &rows);         /* 54 px rows, 6 px gaps */
    CHECK(rows == 12 && pt.x == 500 && pt.y == 700 - 6 - (12 * 54 + 6));
    g_dpi = dpi;
}

/* the saved window position: kept inside the work area, moved fully into it when partly off,
   dropped (default placement) when it lands on no connected monitor */
static void saved_window_position(void)
{
    RECT work = { 0, 0, 1920, 1040 }, wk2 = { -1280, 40, 0, 1024 };
    RECT in = { 100, 200, 760, 1000 }, off_r = { 1500, 300, 2160, 1100 }, off_l = { -50, -30, 610, 770 };
    RECT big = { 10, 10, 2010, 1210 }, left = { -1100, 900, -440, 1700 };
    POINT p = clamp_into(&in, &work);
    CHECK(p.x == 100 && p.y == 200);                          /* inside: kept */
    p = clamp_into(&off_r, &work);
    CHECK(p.x == 1920 - 660 && p.y == 1040 - 800);            /* past the right and the taskbar: moved in */
    p = clamp_into(&off_l, &work);
    CHECK(p.x == 0 && p.y == 0);                              /* past the left and top */
    p = clamp_into(&big, &work);
    CHECK(p.x == 0 && p.y == 0);                              /* larger than the work area: its top-left */
    p = clamp_into(&left, &wk2);
    CHECK(p.x == -1100 && p.y == 1024 - 800);                 /* a monitor left of the primary: negative x */
    RECT gone = { -100000, -100000, -99340, -99200 };         /* on no monitor: default placement */
    POINT q = { 1, 2 };
    CHECK(!saved_place(&gone, &q) && q.x == 1 && q.y == 2);
    MONITORINFO mi;                                           /* on the primary monitor: kept inside it */
    mi.cbSize = sizeof(mi);
    POINT o = { 0, 0 };
    GetMonitorInfoW(MonitorFromPoint(o, MONITOR_DEFAULTTOPRIMARY), &mi);
    RECT prim = { mi.rcWork.left + 10, mi.rcWork.top + 10, mi.rcWork.left + 110, mi.rcWork.top + 110 };
    CHECK(saved_place(&prim, &q) && q.x == prim.left && q.y == prim.top);
    RECT part = { mi.rcWork.right - 70, mi.rcWork.top + 10, mi.rcWork.right + 30, mi.rcWork.top + 110 };
    CHECK(saved_place(&part, &q) && q.x == mi.rcWork.right - 100 && q.y == part.top); /* mostly on it, partly off: in */
}

/* time button and picker columns: hour, minute, AM/PM read left to right on screen in every language
   (AM/PM leftmost on an AM/PM-first clock); a mirrored window's client x grows leftwards, so the
   on-screen x of a client range [l, r) is w - r there */
static int vis_left(int c, int w) { int l, r; tp_col(c, w, &l, &r); return lang_rtl() ? w - r : l; }
static int vis_width(int c, int w) { int l, r; tp_col(c, w, &l, &r); return r - l; }
static void time_columns_read_ltr(void)
{
    int dpi = g_dpi, apw = g_apw, sw = g_sw;
    bool rtl = g_lang_rtl;
    g_dpi = 96; g_apw = 80; g_sw = 242;
    for (int d = 0; d < 2; d++) {
        g_lang_rtl = d == 1;
        g_clock_test = L"h:mm tt";                                 /* hour | minute | AM/PM */
        CHECK(vis_left(0, 242) == 0 && vis_left(1, 242) == 81 && vis_left(2, 242) == 162);
        CHECK(vis_width(0, 242) == 81 && vis_width(1, 242) == 81 && vis_width(2, 242) == 80);
        g_clock_test = L"tt h:mm";                                 /* AM/PM | hour | minute */
        CHECK(vis_left(2, 242) == 0 && vis_left(0, 242) == 80 && vis_left(1, 242) == 161);
        CHECK(vis_width(2, 242) == 80 && vis_width(0, 242) == 81 && vis_width(1, 242) == 81);
        g_clock_test = L"HH:mm";                                   /* hour | minute */
        CHECK(vis_left(0, 242) == 0 && vis_left(1, 242) == 121);
        /* hit-testing: the on-screen leftmost and rightmost cells */
        int c, r, b, xl = g_lang_rtl ? 241 : 0, xr = g_lang_rtl ? 0 : 241;
        g_clock_test = L"h:mm tt";
        tp_hit(xl, 10, &c, &r, &b); CHECK(c == 0);
        tp_hit(xr, 10, &c, &r, &b); CHECK(c == 2);
        g_clock_test = L"tt h:mm";
        tp_hit(xl, 10, &c, &r, &b); CHECK(c == 2);
        tp_hit(xr, 10, &c, &r, &b); CHECK(c == 1);
        /* Left / Right move on screen: from no column, Right takes the leftmost */
        g_clock_test = L"h:mm tt";
        CHECK(tp_next_col(-1, 1) == 0 && tp_next_col(0, 1) == 1 && tp_next_col(2, 1) == 0);
        CHECK(tp_next_col(-1, -1) == 2 && tp_next_col(0, -1) == 2 && tp_next_col(1, -1) == 0);
        g_clock_test = L"tt h:mm";
        CHECK(tp_next_col(-1, 1) == 2 && tp_next_col(2, 1) == 0 && tp_next_col(0, 1) == 1 && tp_next_col(1, 1) == 2);
        CHECK(tp_next_col(-1, -1) == 1 && tp_next_col(1, -1) == 0 && tp_next_col(0, -1) == 2);
    }
    g_clock_test = NULL; g_lang_rtl = rtl; g_dpi = dpi; g_apw = apw; g_sw = sw;
}

/* Enter and Alt+Down open a dropdown once: their auto-repeat (lParam bit 30, key already down) does
   not reopen it after a pick */
static void combo_keys_ignore_repeat(void)
{
    CHECK(combo_open_key(WM_KEYDOWN, VK_RETURN, 0x00000001));
    CHECK(!combo_open_key(WM_KEYDOWN, VK_RETURN, 0x40000001));
    CHECK(combo_open_key(WM_KEYDOWN, VK_F4, 0x00000001));
    CHECK(combo_open_key(WM_SYSKEYDOWN, VK_DOWN, 0x20000001));
    CHECK(!combo_open_key(WM_SYSKEYDOWN, VK_DOWN, 0x60000001));
    CHECK(combo_open_key(WM_SYSKEYDOWN, VK_UP, 0x20000001));
    CHECK(!combo_open_key(WM_KEYDOWN, VK_DOWN, 0x00000001));      /* plain Down: not an opener */
    CHECK(!combo_open_key(WM_KEYDOWN, VK_SPACE, 0x00000001));     /* Space: the button's own click */
}

int main(void)
{
    foot_lang_width();
    fly_above_placement();
    saved_window_position();
    time_columns_read_ltr();
    combo_keys_ignore_repeat();
    korean_wraps_at_spaces();
    task_aliases();
    migration_keeps_unmoved_tasks();
    printf(g_fail ? "%d check(s) FAILED\n" : "all checks passed\n", g_fail);
    return g_fail;
}
