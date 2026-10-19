/* PowerNapp: time picker flyout. Part of the unity build: included by powernapp.c, in order. */

/* ------------------------------------------------------------------ time picker
 * WinUI TimePicker flyout as in the design: the 242x32 segmented button (wider when the
 * AM/PM names need it, layout.c) opens a popup as wide with hour / minute / AM-PM columns (hour / minute on a 24-hour clock)
 * of 40 px rows, the selection in an accent band, wheel or click to change, then accept or cancel. Keyboard: Left /
 * Right pick a column, Up / Down change it, Enter accepts, Esc cancels. Geometry is in
 * design pixels (TP_*), scaled with S(). */

#define TP_CLASS L"PowerNappTimePick"
#define TP_ROW 40
#define TP_COLS (7 * TP_ROW)        /* 280 */
#define TP_H (TP_COLS + 41)         /* + divider + footer */
/* columns: hour / minute / AM-PM (81 / 81 / 80 at the design's 242), or hour / minute halves
   on a 24-hour clock (regional format); the AM/PM column is as wide as its names need (g_apw) */
static int tp_ncols(void) { return clock_24h() ? 2 : 3; }

/* Where column c (0 hour, 1 minute, 2 AM/PM) sits on screen, 0 = leftmost: hour, minute, AM/PM, or
 * AM/PM first when the regional format puts it before the hour ("tt h:mm"), as the time reads in text */
static int tp_slot(int c) {
    if (tp_ncols() == 3 && clock_fmt().ap_first) return c == 2 ? 0 : c + 1;
    return c;
}
static int tp_at_slot(int s) { /* the column at on-screen slot s */
    for (int c = 0, nc = tp_ncols(); c < nc; c++) if (tp_slot(c) == s) return c;
    return 0;
}

/* Column c's edges in client px of a box w px wide (the button body or the popup). Times read
 * left to right in every language, so in a mirrored (right-to-left) window, where client x
 * grows leftwards, the columns are placed from the other side: the hour stays on the left. */
static void tp_col(int c, int w, int *l, int *r) {
    int nc = tp_ncols(), hm = nc == 2 ? w / 2 : (w - g_apw) / 2; /* hour and minute widths */
    int a = 0, s = tp_slot(c);
    for (int k = 0; k <= s; k++) {
        int col = tp_at_slot(k), cw = col == nc - 1 ? w - (nc - 1) * hm : hm; /* the last column: the rest */
        if (k == s) {
            int b = a + cw;
            if (lang_rtl()) { *l = w - b; *r = w - a; }
            else { *l = a; *r = b; }
            return;
        }
        a += cw;
    }
}

/* the on-screen left edge of slot s (1 .. ncols-1) in client px: where a divider goes */
static int tp_divider_x(int s, int w) {
    int l, r;
    tp_col(tp_at_slot(s), w, &l, &r);
    return lang_rtl() ? r : l;
}

/* keyboard Left (dir -1) / Right (+1) from column kcol (-1: none yet) */
static int tp_next_col(int kcol, int dir) { /* by on-screen slot, wrapping */
    int nc = tp_ncols(), s = kcol < 0 ? (dir < 0 ? 0 : -1) : tp_slot(kcol);
    return tp_at_slot(((s + dir) % nc + nc) % nc);
}

static struct {
    HWND wnd, owner;     /* owner: the button that opened it (keeps keyboard focus) */
    int target;          /* 0 = Time row, 1 = Wake time row */
    int h, m;            /* draft, 24h */
    int hcol, hrow;      /* hovered cell, -1 = none */
    int hbtn;            /* hovered footer button: 1 accept, 2 cancel */
    int kcol;            /* keyboard column, -1 until an arrow key is used */
} TP = { NULL, NULL, 0, 0, 0, -1, -1, 0, -1 };

static void paint_time_btn(HDC hdc, HWND btn, int h, int m) {
    RECT all, rc = ctl_body(btn);
    GetClientRect(btn, &all);
    fill_rect_c(hdc, &all, T_CARD);
    focus_ring(hdc, &rc, 4, T_CARD);
    fill_round(hdc, &rc, 4, btn == G.hover_btn ? T_CTLHOV : T_CTL, T_CTLBRD);
    int x0 = rc.left, top = rc.top, bot = rc.bottom, nc = tp_ncols(), bw = rc.right - rc.left;
    for (int c = 1; c < nc; c++) { /* dividers: each slot's on-screen left edge */
        int x = x0 + tp_divider_x(c, bw);
        line_c(hdc, x, top + SB(1), x, bot - SB(1) - 1, T_CTLBRD, 1);
    }
    line_c(hdc, x0 + S(3), bot - SB(1) - 1, rc.right - S(4), bot - SB(1) - 1, T_CTLBOT, 1);
    wchar_t t[3][16];
    fmt_hour(h, t[0], 16);
    StringCchPrintfW(t[1], 16, L"%02d", m);
    ampm(h >= 12, t[2], 16);
    UINT f = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
    for (int c = 0; c < nc; c++) {
        int l, r;
        tp_col(c, bw, &l, &r);
        RECT s = { x0 + l, top, x0 + r, bot };
        text_ltr(hdc, t[c], s, G.f_row, T_T1, f);
    }
}

static void tp_label(int col, int off, wchar_t *out, size_t n) {
    out[0] = L'\0';
    if (col == 0) {
        fmt_hour(clock_24h() ? ((TP.h + off) % 24 + 24) % 24 : ((TP.h % 12 + off) % 12 + 12) % 12, out, n);
    } else if (col == 1) {
        StringCchPrintfW(out, n, L"%02d", ((TP.m + off) % 60 + 60) % 60);
    } else { /* AM/PM: only the other value, on its side */
        bool pm = TP.h >= 12;
        if (off == 0) ampm(pm, out, n);
        else if (off == 1 && !pm) ampm(true, out, n);
        else if (off == -1 && pm) ampm(false, out, n);
    }
}

static void tp_shift(int col, int d) {
    if (col == 0 && clock_24h()) TP.h = ((TP.h + d) % 24 + 24) % 24;
    else if (col == 0) TP.h = ((TP.h % 12 + d) % 12 + 12) % 12 + (TP.h >= 12 ? 12 : 0);
    else if (col == 1) TP.m = ((TP.m + d) % 60 + 60) % 60;
    else if ((d > 0 && TP.h < 12) || (d < 0 && TP.h >= 12)) TP.h = (TP.h + 12) % 24;
    InvalidateRect(TP.wnd, NULL, FALSE);
}

static void acc_time_btn(HWND btn, int h, int m); /* accessibility value, window.c */

static void tp_close(void) {
    if (!TP.wnd) return;
    HWND w = TP.wnd;
    TP.wnd = NULL; /* first: ReleaseCapture sends WM_CAPTURECHANGED back to us */
    ReleaseCapture();
    DestroyWindow(w);
    acc_sync_all(); /* drop the announced draft time if it wasn't accepted */
}

static void tp_accept(void) {
    int target = TP.target;
    if (target == 0) {
        G.rt.plan.h = TP.h;
        G.rt.plan.m = TP.m;
        save_current();
    } else {
        G.wake_h = TP.h;
        G.wake_m = TP.m;
        G.wake_dirty = true;
    }
    tp_close();
    if (target == 1) wake_flush(); /* re-registers the wake task */
    HWND btn = target == 0 ? G.time_btn : G.wake_btn;
    acc_time_btn(btn, target == 0 ? G.rt.plan.h : G.wake_h, target == 0 ? G.rt.plan.m : G.wake_m);
    InvalidateRect(G.main, NULL, FALSE); /* time-row subtitle */
    InvalidateRect(btn, NULL, FALSE);
}

static void tp_hit(int x, int y, int *col, int *row, int *btn) {
    *col = *row = -1;
    *btn = 0;
    if (x < 0 || x >= g_sw || y < 0 || y >= S(TP_H)) return;
    if (y < S(TP_COLS)) {
        for (int c = 0, nc = tp_ncols(); c < nc; c++) {
            int l, r;
            tp_col(c, g_sw, &l, &r);
            if (x >= l && x < r) *col = c;
        }
        *row = y / S(TP_ROW);
    } else if (y > S(TP_COLS + 4) && y < S(TP_H - 4)) {
        *btn = x < g_sw / 2 ? 1 : 2;
    }
}

/* keyboard, forwarded by the owner button (the popup never takes focus) */
static bool tp_key(WPARAM vk) {
    if (!TP.wnd) return false;
    switch (vk) {
    case VK_ESCAPE: tp_close(); return true;
    case VK_RETURN: case VK_SPACE: tp_accept(); return true;
    case VK_LEFT: TP.kcol = tp_next_col(TP.kcol, -1); break;  /* on screen, in every language */
    case VK_RIGHT: case VK_TAB: TP.kcol = tp_next_col(TP.kcol, 1); break;
    case VK_UP: if (TP.kcol < 0) TP.kcol = 0; tp_shift(TP.kcol, -1); break;
    case VK_DOWN: if (TP.kcol < 0) TP.kcol = 0; tp_shift(TP.kcol, 1); break;
    default: return false;
    }
    acc_time_btn(TP.owner, TP.h, TP.m); /* announce the draft time */
    InvalidateRect(TP.wnd, NULL, FALSE);
    return true;
}

static LRESULT CALLBACK tp_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        int W = g_sw, H = S(TP_H), R = S(TP_ROW), band = 3 * R;
        PAINTSTRUCT ps;
        HDC wdc = BeginPaint(hwnd, &ps);
        HDC hdc = CreateCompatibleDC(wdc); /* buffered: aa_rrect reads the DC back */
        HBITMAP bm = CreateCompatibleBitmap(wdc, W, H);
        HGDIOBJ ob = SelectObject(hdc, bm);
        dc_match(hdc, wdc);
        RECT all = { 0, 0, W, H };
        fill_rect_c(hdc, &all, T_FLY);
        aa_rrect(hdc, SF(4), (float)band, (float)(W - S(8)), (float)R, SF(4), T_ACCENT, T_ACCENT, 0);
        int nc = tp_ncols();
        for (int c = 0; c < nc; c++) {
            int cl, cr_;
            tp_col(c, W, &cl, &cr_);
            if (c == TP.kcol) /* keyboard column */
                aa_rrect(hdc, (float)cl + SF(2), SF(2), (float)(cr_ - cl) - SF(4), (float)(7 * R) - SF(4), SF(4),
                    T_FLY, T_T1, SF(2));
            for (int r = 0; r < 7; r++) {
                int off = r - 3;
                wchar_t t[16];
                tp_label(c, off, t, 16);
                if (!t[0]) continue;
                RECT cell = { cl, r * R, cr_, r * R + R };
                if (off && c == TP.hcol && r == TP.hrow)
                    aa_rrect(hdc, (float)cl + SF(4), (float)cell.top, (float)(cr_ - cl) - SF(8), (float)R, SF(4),
                        T_SUBTLE, T_SUBTLE, 0);
                text_ltr(hdc, t, cell, G.f_row, off ? T_T1 : T_ONACC, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
        for (int c = 1; c < nc; c++) { /* column dividers, not across the band */
            int x = tp_divider_x(c, W);
            line_c(hdc, x, 0, x, band, T_FLYBRD, 1);
            line_c(hdc, x, band + R, x, 7 * R, T_FLYBRD, 1);
        }
        line_c(hdc, 0, S(TP_COLS), W, S(TP_COLS), T_FLYBRD, 1);
        RECT ok = { S(4), S(TP_COLS + 5), W / 2 - S(2), H - S(4) };
        RECT no = { W / 2 + S(2), S(TP_COLS + 5), W - S(4), H - S(4) };
        if (TP.hbtn == 1) fill_round(hdc, &ok, 4, T_SUBTLE, T_SUBTLE);
        if (TP.hbtn == 2) fill_round(hdc, &no, 4, T_SUBTLE, T_SUBTLE);
        glyph_at(hdc, 0xE73E, ok, 14, T_T1); /* CheckMark */
        glyph_at(hdc, 0xE711, no, 14, T_T1); /* Cancel */
        BitBlt(wdc, 0, 0, W, H, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, ob);
        DeleteObject(bm);
        DeleteDC(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE: {
        int c, r, b;
        tp_hit(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), &c, &r, &b);
        if (c != TP.hcol || r != TP.hrow || b != TP.hbtn) {
            TP.hcol = c; TP.hrow = r; TP.hbtn = b;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int c, r, b, x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (x < 0 || x >= g_sw || y < 0 || y >= S(TP_H)) { tp_close(); return 0; } /* click outside */
        tp_hit(x, y, &c, &r, &b);
        if (b == 1) tp_accept();
        else if (b == 2) tp_close();
        else if (c >= 0 && r != 3) {
            wchar_t t[16];
            tp_label(c, r - 3, t, 16);
            if (t[0]) tp_shift(c, r - 3);
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &pt);
        int c, r, b;
        tp_hit(pt.x, pt.y, &c, &r, &b);
        if (c >= 0) tp_shift(c, GET_WHEEL_DELTA_WPARAM(wp) < 0 ? 1 : -1);
        return 0;
    }
    case WM_KEYDOWN:
        tp_key(wp);
        return 0;
    case WM_CAPTURECHANGED:
        if (TP.wnd == hwnd && (HWND)lp != hwnd) tp_close();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool popup_open(void) { return FLY.wnd || TP.wnd; }
static bool popup_key(WPARAM vk) { return FLY.wnd ? fly_key(vk) : tp_key(vk); }

static void tp_open(HWND btn, int target, int h, int m) {
    tp_close();
    fly_close();
    RECT br = ctl_body(btn);
    MapWindowPoints(btn, NULL, (POINT *)&br, 2);
    int W = g_sw, H = S(TP_H);
    int x = br.left, y = (br.top + br.bottom) / 2 - (3 * S(TP_ROW) + S(TP_ROW) / 2); /* band on the button */
    HMONITOR mon = MonitorFromWindow(btn, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);
    if (y < mi.rcWork.top) y = mi.rcWork.top;
    if (y + H > mi.rcWork.bottom) y = mi.rcWork.bottom - H;
    if (x + W > mi.rcWork.right) x = mi.rcWork.right - W;
    if (x < mi.rcWork.left) x = mi.rcWork.left;
    TP.owner = btn;
    TP.target = target;
    TP.h = h;
    TP.m = m;
    TP.hcol = TP.hrow = -1;
    TP.hbtn = 0;
    TP.kcol = -1;
    TP.wnd = CreateWindowExW(WS_EX_TOOLWINDOW | wnd_ex_dir(), TP_CLASS, NULL, WS_POPUP, x, y, W, H,
        G.main, NULL, GetModuleHandleW(NULL), NULL);
    if (!TP.wnd) return;
    DWORD corner = 2; /* DWMWCP_ROUND: rounded, shadowed, system border */
    DwmSetWindowAttribute(TP.wnd, 33, &corner, sizeof(corner));
    ShowWindow(TP.wnd, SW_SHOWNOACTIVATE);
    SetCapture(TP.wnd);
}
