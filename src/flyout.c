/* PowerNapp: dropdown flyout. Part of the unity build: included by powernapp.c, in order. */

/* ------------------------------------------------------------------ flyout (custom WinUI-style dropdown) */

#define FLY_CLASS L"PowerNappFly"

typedef struct {
    const wchar_t *label;
    IconId ic;
    COLORREF color;
} FlyItem;

#define FLY_MAX 48   /* the language list: Windows default + English + every translation */
#define FLY_ROWS 12  /* longer lists scroll (wheel, arrows, Page Up/Down) */

static struct {
    HWND wnd, owner;
    int combo_id;
    FlyItem items[FLY_MAX];
    int count, sel, hover, pick;
    int top, rows; /* first visible item, visible items */
} FLY = { .pick = -1 }; /* -1: no pending pick (0 would read as "first item picked") */

static int fly_clamp_top(int top) {
    if (top > FLY.count - FLY.rows) top = FLY.count - FLY.rows;
    return top < 0 ? 0 : top;
}

static int fly_item_at(int y) { /* client y -> item index, -1 outside */
    int r = (y - S(2)) / S(36);
    if (y < S(2) || r >= FLY.rows || FLY.top + r >= FLY.count) return -1;
    return FLY.top + r;
}

static void fly_scroll(int top) {
    top = fly_clamp_top(top);
    if (top == FLY.top) return;
    FLY.top = top;
    InvalidateRect(FLY.wnd, NULL, FALSE);
}

static void acc_sync_all(void); /* window.c */

static void fly_close(void) {
    if (FLY.wnd) {
        HWND w = FLY.wnd;
        FLY.wnd = NULL;
        ReleaseCapture();
        DestroyWindow(w);
        acc_sync_all(); /* drop the announced arrow-key value if nothing was picked */
    }
}

/* design: 208 wide, or the control's width + 8, or the widest item + its 2 x 16 px padding */
static int fly_width(const RECT *btn_rc, const FlyItem *items, int count) {
    int w = max_i(S(208), btn_rc->right - btn_rc->left + S(8));
    for (int i = 0; i < count; i++) w = max_i(w, text_px(G.f_row, items[i].label) + S(32));
    return w;
}

/* below: drop down under the control, aligned with its trailing edge (the title bar's Language
   button, at the window's trailing end); otherwise over it, the selected item on the control */
static void fly_open(HWND owner, int combo_id, const FlyItem *items, int count, int sel,
        const RECT *btn_rc, bool below) {
    fly_close();
    FLY.owner = owner;
    FLY.combo_id = combo_id;
    if (count > FLY_MAX) count = FLY_MAX;
    FLY.count = count;
    FLY.sel = sel;
    FLY.hover = -1;
    FLY.pick = -1;
    for (int i = 0; i < count; i++) FLY.items[i] = items[i];
    /* design: 208 wide (or the control's width + 8), 2 px padding, 36 px items, opened
     * over the control so the selected item sits on it: left -4, top -(sel*36 + 6).
     * A list longer than FLY_ROWS (or the screen) scrolls, the selection near the middle. */
    int ih = S(36), w = fly_width(btn_rc, items, count);
    RECT sr = *btn_rc; /* to screen; both corners at once keep left < right from a mirrored owner */
    MapWindowPoints(owner, NULL, (POINT *)&sr, 2);
    /* over the control from its leading edge: the left one, or the right one in right-to-left */
    POINT pt = { lang_rtl() ? sr.right + S(4) - w : sr.left - S(4), sr.top };
    if (below) { pt.x = lang_rtl() ? sr.left : sr.right - w; pt.y = sr.bottom + S(4); }
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);
    int fit = (mi.rcWork.bottom - mi.rcWork.top - S(4)) / ih;
    FLY.rows = count < FLY_ROWS ? count : FLY_ROWS;
    if (FLY.rows > fit && fit > 0) FLY.rows = fit;
    FLY.top = fly_clamp_top(sel - FLY.rows / 2);
    int h = FLY.rows * ih + S(4);
    if (!below) pt.y -= (sel - FLY.top) * ih + S(6);
    if (pt.y + h > mi.rcWork.bottom) pt.y = mi.rcWork.bottom - h;
    if (pt.y < mi.rcWork.top) pt.y = mi.rcWork.top;
    if (pt.x + w > mi.rcWork.right) pt.x = mi.rcWork.right - w;
    if (pt.x < mi.rcWork.left) pt.x = mi.rcWork.left;
    FLY.wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | wnd_ex_dir(), FLY_CLASS, NULL,
        WS_POPUP, pt.x, pt.y, w, h, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!FLY.wnd) return;
    /* let DWM round it (anti-aliased, with the system border); a window region
     * would clip hard and stair-step. Windows 10 ignores this: square corners. */
    DWORD corner = 2; /* DWMWCP_ROUND */
    DwmSetWindowAttribute(FLY.wnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corner, sizeof(corner));
    ShowWindow(FLY.wnd, SW_SHOWNOACTIVATE);
    SetCapture(FLY.wnd);
}

static void fly_pick(int idx) { /* report like a click: WM_COMMAND with the index in lp */
    FLY.pick = idx;
    int cid = FLY.combo_id;
    HWND owner = FLY.owner;
    fly_close();
    PostMessageW(owner, WM_COMMAND, MAKEWPARAM(cid, 0), (LPARAM)idx);
}

/* wheel, from the popup or forwarded by the main window: 3 items a notch */
static void fly_wheel(int delta) {
    static int rest; /* touchpads send fractions of a notch */
    if (!FLY.wnd) return;
    rest += delta * 3;
    int n = rest / WHEEL_DELTA;
    rest -= n * WHEEL_DELTA;
    fly_scroll(FLY.top - n);
}

/* keyboard, forwarded by the combo button (the popup never takes focus) */
static bool fly_key(WPARAM vk) {
    if (!FLY.wnd) return false;
    int cur = FLY.hover >= 0 ? FLY.hover : FLY.sel;
    switch (vk) {
    case VK_ESCAPE: fly_close(); return true;
    case VK_RETURN: case VK_SPACE: fly_pick(cur); return true;
    case VK_UP: FLY.hover = cur > 0 ? cur - 1 : 0; break;
    case VK_DOWN: FLY.hover = cur < FLY.count - 1 ? cur + 1 : FLY.count - 1; break;
    case VK_PRIOR: FLY.hover = cur - FLY.rows > 0 ? cur - FLY.rows : 0; break;
    case VK_NEXT: FLY.hover = cur + FLY.rows < FLY.count - 1 ? cur + FLY.rows : FLY.count - 1; break;
    case VK_HOME: FLY.hover = 0; break;
    case VK_END: FLY.hover = FLY.count - 1; break;
    default: return false;
    }
    if (FLY.hover < FLY.top) FLY.top = FLY.hover; /* keep it in view */
    else if (FLY.hover >= FLY.top + FLY.rows) FLY.top = FLY.hover - FLY.rows + 1;
    InvalidateRect(FLY.wnd, NULL, FALSE);
    acc_value(GetDlgItem(FLY.owner, FLY.combo_id), FLY.items[FLY.hover].label); /* announce */
    return true;
}

/* the open list, into hdc (client coordinates of the popup, rc its client rect) */
static void fly_paint(HDC hdc, const RECT *rc) {
    fill_rect_c(hdc, rc, T_FLY); /* DWM rounds the window and draws its border */
    for (int i = FLY.top; i < FLY.top + FLY.rows; i++) {
        int r = i - FLY.top;
        RECT ir = { S(4), S(2) + r * S(36), rc->right - S(4), S(2) + (r + 1) * S(36) };
        if (i == FLY.sel || i == FLY.hover)
            fill_round(hdc, &ir, 4, T_SUBTLE, T_SUBTLE);
        if (i == FLY.sel) /* WinUI selection pill */
            aa_rrect(hdc, (float)ir.left, (float)ir.top + SF(10), SF(3), SF(16), SF(1.5f), T_ACCENT, T_ACCENT, 0);
        RECT tr = { ir.left + S(12), ir.top, ir.right - S(12), ir.bottom };
        text_at(hdc, FLY.items[i].label, tr, G.f_row, T_T1,
            DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    if (FLY.rows < FLY.count) { /* WinUI's collapsed scroll bar: a thin thumb at the right */
        float track = (float)(rc->bottom - S(8)), th = track * FLY.rows / FLY.count;
        float ty = SF(4) + track * FLY.top / FLY.count;
        aa_rrect(hdc, (float)rc->right - SF(4), ty, SF(2), th, SF(1), T_T2, T_T2, 0);
    }
}

static LRESULT CALLBACK fly_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC wdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HDC hdc = CreateCompatibleDC(wdc); /* buffered: aa_rrect blends with what's below */
        HBITMAP bm = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
        HGDIOBJ ob = SelectObject(hdc, bm);
        dc_match(hdc, wdc);
        fly_paint(hdc, &rc);
        BitBlt(wdc, 0, 0, rc.right, rc.bottom, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, ob);
        DeleteObject(bm);
        DeleteDC(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        RECT rc;
        GetClientRect(hwnd, &rc);
        int idx = fly_item_at(pt.y);
        if (pt.x < 0 || pt.x > rc.right) idx = -1;
        if (idx != FLY.hover) {
            FLY.hover = idx;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        RECT rc;
        GetClientRect(hwnd, &rc);
        int idx = fly_item_at(pt.y);
        if (idx >= 0 && pt.x >= 0 && pt.x <= rc.right) {
            fly_pick(idx);
        } else {
            fly_close();
        }
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (FLY.wnd == hwnd) fly_close();
        return 0;
    case WM_KILLFOCUS:
        fly_close();
        return 0;
    case WM_KEYDOWN:
        fly_key(wp);
        return 0;
    case WM_MOUSEWHEEL:
        fly_wheel(GET_WHEEL_DELTA_WPARAM(wp));
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
