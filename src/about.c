/* PowerNapp: About dialog. Part of the unity build: included by powernapp.c, in order. */

/* ------------------------------------------------------------------ about
 * ContentDialog-style box: icon + name, version, description, "Contact us", a link to
 * more apps, copyright, and a footer band with "Check for updates" and an accent "OK".
 * Modal to the window. Keyboard: Tab / arrows move between the controls, Enter / Space
 * press, Esc closes. Geometry in design pixels, scaled with S(). */

#define ABOUT_CLASS L"PowerNappAbout"
#define APP_VERSION L"2.0.0"
#define RELEASES_URL L"https://github.com/nappsllc/PowerNapp/releases/latest"
#define CONTACT_URL L"mailto:hi@powernapps.net?subject=PowerNapps%20PowerNapp"
#define SITE_URL L"https://powernapps.net"
#define AB_W 400
#define AB_H 396
#define AB_FOOT 316  /* footer band top */
enum { AB_CONTACT = 1, AB_SITE, AB_UPDATES, AB_OK, AB_N = AB_OK };
static const StrId AB_NAMES[] = { L_COUNT, L_ABOUT_CONTACT_ACC, L_ABOUT_LINK_ACC, L_ABOUT_UPDATES_ACC, L_ABOUT_OK_ACC };
static struct { HWND wnd; int hover, focus, w, h, dy, cw, lw; } AB; /* w h: size; dy: extra description lines; cw lw: Contact, link widths */

/* Sized to the language: as wide as its longest line (400 in the design), taller when the
 * description needs more than 3 lines, Contact us as wide as its text + 40. */
static void ab_measure(void) {
    wchar_t vs[64];
    StringCchPrintfW(vs, 64, TR(L_ABOUT_VERSION), APP_VERSION);
    const wchar_t *line[] = { TR(L_ABOUT_LINK), TR(L_ABOUT_CONTACT), vs, TR(L_ABOUT_ASK), TR(L_ABOUT_COPYRIGHT) };
    int w = S(AB_W - 48);
    for (int i = 0; i < 5; i++) w = max_i(w, text_px(G.f_row, line[i]));
    AB.lw = text_px(G.f_row, line[0]);
    AB.cw = max_i(S(120), text_px(G.f_row, line[1]) + S(40));
    AB.dy = max_i(0, text_ext(G.f_row, TR(L_ABOUT_DESC), w) - S(60));
    AB.w = w + S(48);
    AB.h = S(AB_H) + AB.dy;
}

static RECT ab_btn(int i) {
    RECT r = { 0 };
    switch (i) {
    case AB_CONTACT: r = (RECT){ S(24), S(200), S(24) + AB.cw, S(232) }; break;
    case AB_SITE: r = (RECT){ S(24), S(248), S(24) + AB.lw, S(268) }; break; /* as wide as its text */
    case AB_UPDATES: r = (RECT){ S(24), S(340), AB.w / 2 - S(8), S(372) }; break;
    case AB_OK: r = (RECT){ AB.w / 2 + S(8), S(340), AB.w - S(24), S(372) }; break;
    }
    OffsetRect(&r, 0, AB.dy); /* all below the description */
    return r;
}

static void about_close(void) {
    if (!AB.wnd) return;
    HWND w = AB.wnd;
    AB.wnd = NULL;
    EnableWindow(G.main, TRUE); /* before destroying, so activation returns to the window */
    DestroyWindow(w);
}

static int about_hit(int x, int y) {
    POINT pt = { x, y };
    for (int i = 1; i <= AB_N; i++) {
        RECT r = ab_btn(i);
        if (PtInRect(&r, pt)) return i;
    }
    return 0;
}

static void about_press(int i) {
    static const wchar_t *URL[] = { NULL, CONTACT_URL, SITE_URL, RELEASES_URL };
    wchar_t pfn[128], store[192];
    UINT32 n = 128;
    if (i == AB_UPDATES && packaged() && GetCurrentPackageFamilyName(&n, pfn) == ERROR_SUCCESS) {
        StringCchPrintfW(store, 192, L"ms-windows-store://pdp/?PFN=%ls", pfn); /* the Store updates it */
        ShellExecuteW(NULL, L"open", store, NULL, NULL, SW_SHOWNORMAL);
        return;
    }
    if (i == AB_OK) about_close();
    else if (i) ShellExecuteW(NULL, L"open", URL[i], NULL, NULL, SW_SHOWNORMAL);
}

/* the dialog, into hdc */
static void about_paint(HDC hdc) {
    int W = AB.w, H = AB.h, ic = S(32), dy = AB.dy;
    HICON icon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, ic, ic, 0);
    RECT all = { 0, 0, W, H }, foot = { 0, S(AB_FOOT) + dy, W, H };
    fill_rect_c(hdc, &all, T_CARD);
    fill_rect_c(hdc, &foot, T_WIN);
    line_c(hdc, 0, foot.top, W, foot.top, T_CARDBRD, 1);
    if (icon) { DrawIconEx(hdc, S(24), S(24), icon, ic, ic, 0, NULL, DI_NORMAL | DI_NOMIRROR); DestroyIcon(icon); }
    RECT tr = { S(68), S(24), W - S(24), S(56) };
    text_at(hdc, APP_NAME, tr, G.f_hero, T_T1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT ver = { S(24), S(72), W - S(24), S(92) }, desc = { S(24), S(100), W - S(24), S(160) + dy },
         ask = { S(24), S(172) + dy, W - S(24), S(192) + dy }, copy = { S(24), S(276) + dy, W - S(24), S(296) + dy };
    wchar_t vs[64];
    StringCchPrintfW(vs, 64, TR(L_ABOUT_VERSION), APP_VERSION);
    text_at(hdc, vs, ver, G.f_row, T_T1, DT_LEFT | DT_SINGLELINE);
    text_at(hdc, TR(L_ABOUT_DESC), desc, G.f_row, T_T2, DT_LEFT | DT_WORDBREAK);
    text_at(hdc, TR(L_ABOUT_ASK), ask, G.f_row, T_T1, DT_LEFT | DT_SINGLELINE);
    text_at(hdc, TR(L_ABOUT_COPYRIGHT), copy, G.f_row, T_T2, DT_LEFT | DT_SINGLELINE);
    RECT con = ab_btn(AB_CONTACT), site = ab_btn(AB_SITE), upd = ab_btn(AB_UPDATES), ok = ab_btn(AB_OK);
    g_focus_cues = true; /* keyboard-only dialog: the focused control always shows it */
    RECT f = ab_btn(AB.focus);
    focus_ring(hdc, &f, AB.focus == AB_SITE ? 2 : 4, AB.focus >= AB_UPDATES ? T_WIN : T_CARD);
    fill_round(hdc, &con, 4, AB.hover == AB_CONTACT ? T_CTLHOV : T_CTL, T_CTLBRD);
    text_at(hdc, TR(L_ABOUT_CONTACT), con, G.f_row, T_T1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    text_at(hdc, TR(L_ABOUT_LINK), site, G.f_row, AB.hover == AB_SITE ? T_ACCHOV : T_ACCENT,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
    fill_round(hdc, &upd, 4, AB.hover == AB_UPDATES ? T_CTLHOV : T_CTL, T_CTLBRD);
    text_at(hdc, TR(L_ABOUT_UPDATES), upd, G.f_row, T_T1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    fill_round(hdc, &ok, 4, AB.hover == AB_OK ? T_ACCHOV : T_ACCENT, T_ACCENT);
    text_at(hdc, TR(L_ABOUT_OK), ok, G.f_row, T_ONACC, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static LRESULT CALLBACK about_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        int W = AB.w, H = AB.h;
        PAINTSTRUCT ps;
        HDC wdc = BeginPaint(hwnd, &ps);
        HDC hdc = CreateCompatibleDC(wdc);
        HBITMAP bm = CreateCompatibleBitmap(wdc, W, H);
        HGDIOBJ ob = SelectObject(hdc, bm);
        dc_match(hdc, wdc);
        about_paint(hdc);
        BitBlt(wdc, 0, 0, W, H, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, ob);
        DeleteObject(bm);
        DeleteDC(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR: /* the hand over the link, as on a web page */
        if (AB.hover == AB_SITE) { SetCursor(LoadCursorW(NULL, IDC_HAND)); return TRUE; }
        break;
    case WM_MOUSEMOVE: {
        int h = about_hit(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (h != AB.hover) { AB.hover = h; InvalidateRect(hwnd, NULL, FALSE); }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (AB.hover) { AB.hover = 0; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_LBUTTONUP:
        about_press(about_hit(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
        return 0;
    case WM_KEYDOWN:
        switch (wp) {
        case VK_ESCAPE: about_close(); break;
        case VK_RETURN: case VK_SPACE: about_press(AB.focus); break;
        case VK_TAB: case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: {
            UINT left = lang_rtl() ? VK_RIGHT : VK_LEFT; /* mirrored: the previous control is on the right */
            bool back = wp == left || wp == VK_UP || (wp == VK_TAB && GetKeyState(VK_SHIFT) < 0);
            AB.focus = back ? (AB.focus == 1 ? AB_N : AB.focus - 1) : (AB.focus == AB_N ? 1 : AB.focus + 1);
            acc_desc(hwnd, TR(AB_NAMES[AB.focus]));
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }
        }
        return 0;
    case WM_CLOSE:
        about_close();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void about_open(void) {
    if (AB.wnd) { SetForegroundWindow(AB.wnd); return; }
    RECT mr;
    GetWindowRect(G.main, &mr);
    ab_measure();
    int W = AB.w, H = AB.h;
    int x = (mr.left + mr.right - W) / 2, y = (mr.top + mr.bottom - H) / 2;
    AB.hover = 0;
    AB.focus = AB_OK; /* as a ContentDialog's default button */
    AB.wnd = CreateWindowExW(wnd_ex_dir(), ABOUT_CLASS, TR(L_ABOUT_TITLE), WS_POPUP, x, y, W, H,
        G.main, NULL, GetModuleHandleW(NULL), NULL);
    if (!AB.wnd) return;
    DWORD corner = 2; /* DWMWCP_ROUND */
    DwmSetWindowAttribute(AB.wnd, 33, &corner, sizeof(corner));
    wchar_t ver[64], desc[1024];
    StringCchPrintfW(ver, 64, TR(L_ABOUT_VERSION), APP_VERSION);
    StringCchPrintfW(desc, 1024, TR(L_ABOUT_ACC), ver, TR(L_ABOUT_DESC), TR(L_ABOUT_ASK), TR(L_ABOUT_COPYRIGHT),
        TR(L_ABOUT_CONTACT), TR(L_ABOUT_LINK), TR(L_ABOUT_UPDATES), TR(L_ABOUT_OK));
    acc_control(AB.wnd, TR(L_ABOUT_TITLE), ROLE_SYSTEM_DIALOG, desc);
    EnableWindow(G.main, FALSE);
    ShowWindow(AB.wnd, SW_SHOW);
    SetForegroundWindow(AB.wnd);
    SetFocus(AB.wnd);
}
