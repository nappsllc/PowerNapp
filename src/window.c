/* PowerNapp: title bar, main window, GUI startup. Part of the unity build: included by powernapp.c, in order. */

/* ------------------------------------------------------------------ main window proc */

static void ensure_tray(void);
static void recreate_main(void);
static bool g_recreating; /* the window is being rebuilt: its WM_DESTROY must not quit */

/* ------------------------------------------------------------------ title bar
 * The design's 48 px title bar: Windows has no taller standard caption, so the
 * client area extends over it (WM_NCCALCSIZE) and it is drawn here. The strip
 * still hit-tests as HTCAPTION, so dragging, Snap and the system menu work. */

static RECT cap_btn_rect(int i) { /* i = 1 min, 2 max, 3 close; 46x32 at top right */
    RECT cr;
    GetClientRect(G.main, &cr);
    RECT r = { cr.right - S(46) * (4 - i), 0, cr.right - S(46) * (3 - i), S(32) };
    return r;
}

/* the live caption button under pt: 1 min or 3 close. Maximize (2) is drawn disabled (fixed-size
   window) and is plain title bar: it never hovers or presses, and drags like the rest. */
static int cap_btn_at(POINT pt) {
    for (int i = 1; i <= 3; i += 2) {
        RECT r = cap_btn_rect(i);
        if (PtInRect(&r, pt)) return i;
    }
    return 0;
}

/* the live buttons answer hit-tests as the system's own (HTMINBUTTON, HTCLOSE); never HTMAXBUTTON,
   which on Windows 11 would offer the Snap Layouts flyout for a window that can't resize */
static const LRESULT CAP_HT[4] = { HTCAPTION, HTMINBUTTON, HTCAPTION, HTCLOSE };
static int cap_btn_of_ht(WPARAM ht) {
    return ht == HTMINBUTTON ? 1 : ht == HTCLOSE ? 3 : 0;
}

static void invalidate_title(void) {
    RECT cr;
    GetClientRect(G.main, &cr);
    RECT t = { 0, 0, cr.right, S(TITLE_H) };
    InvalidateRect(G.main, &t, FALSE);
}

static void paint_title(HDC hdc) {
    static HICON icon;
    static int icon_px;
    if (icon_px != S(16)) { /* reload at the current DPI */
        if (icon) DestroyIcon(icon);
        icon_px = S(16);
        icon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, icon_px, icon_px, 0);
    }
    if (icon) DrawIconEx(hdc, S(16), S(16), icon, icon_px, icon_px, 0, NULL, DI_NORMAL | DI_NOMIRROR);
    RECT cr;
    GetClientRect(G.main, &cr);
    COLORREF fg = G.inactive ? T_T2 : T_T1;
    RECT tr = { S(16 + 16 + 12), 0, cr.right - S(46 * 3 + 8), S(TITLE_H) };
    text_at(hdc, APP_NAME, tr, G.f_sub, fg, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    static const wchar_t glyph[4] = { 0, 0xE921, 0xE922, 0xE8BB }; /* ChromeMinimize/Maximize/Close */
    for (int i = 1; i <= 3; i++) {
        RECT r = cap_btn_rect(i);
        bool enabled = i != 2; /* fixed-size window: maximize disabled */
        bool hot = enabled && G.cap_hover == i, down = hot && G.cap_down == i;
        COLORREF c = enabled ? fg : mix_c(T_WIN, T_T1, 0.36f);
        if (hot && i == 3) {
            fill_rect_c(hdc, &r, down ? RGB(200, 64, 49) : RGB(196, 43, 28));
            c = RGB(255, 255, 255);
        } else if (hot) {
            fill_rect_c(hdc, &r, down ? mix_c(T_WIN, T_T1, 0.03f) : T_SUBTLE);
        }
        glyph_at(hdc, glyph[i], r, 10, c);
    }
}

static const wchar_t *g_ui_face; /* the face the UI fonts were made with (ui_font_face) */

static void make_ui_fonts(void) { /* at the current DPI, in the language's face */
    g_ui_face = ui_font_face(lang_tag());
    G.f_hero = make_font(g_ui_face, L"Segoe UI", -20, FW_SEMIBOLD);
    G.f_row = make_font(g_ui_face, L"Segoe UI", -14, FW_NORMAL);
    G.f_sub = make_font(g_ui_face, L"Segoe UI", -12, FW_NORMAL);
    G.f_hdr = make_font(g_ui_face, L"Segoe UI", -14, FW_SEMIBOLD); /* section headers */
}

static void free_ui_fonts(void) {
    HFONT *fonts[] = { &G.f_hero, &G.f_row, &G.f_sub, &G.f_hdr };
    for (int i = 0; i < 4; i++) { if (*fonts[i]) DeleteObject(*fonts[i]); *fonts[i] = NULL; }
}

/* ------------------------------------------------------------------ accessibility state
 * Names, roles and descriptions for the owner-drawn controls, and their current values
 * and checked states. Cheap (a few COM calls), so it simply runs after every change. */

static void acc_time_btn(HWND btn, int h, int m) {
    wchar_t v[16];
    fmt_time(h, m, v, 16);
    acc_value(btn, v);
}

static void acc_sync_all(void) {
    if (!G.main || !g_acc) return;
    wchar_t v[128];
    acc_control(G.combo_action, TR(L_ACTION_TITLE), ROLE_SYSTEM_COMBOBOX, TR(L_ACTION_SUB));
    acc_value(G.combo_action, TASK_TR(G.rt.plan.task));
    acc_control(G.combo_mode, TR(L_SCHED_TITLE), ROLE_SYSTEM_COMBOBOX, TR(L_SCHED_SUB));
    get_text(G.combo_mode, v, 128); acc_value(G.combo_mode, v);
    acc_control(G.combo_cd, TR(L_ACC_COUNTDOWN), ROLE_SYSTEM_COMBOBOX, NULL);
    get_text(G.combo_cd, v, 128); acc_value(G.combo_cd, v);
    acc_control(G.combo_idle, TR(L_ACC_IDLE), ROLE_SYSTEM_COMBOBOX, NULL);
    get_text(G.combo_idle, v, 128); acc_value(G.combo_idle, v);
    acc_control(G.time_btn, TR(L_TIME_TITLE), ROLE_SYSTEM_COMBOBOX, TR(L_ACC_TIME));
    acc_time_btn(G.time_btn, G.rt.plan.h, G.rt.plan.m);
    acc_control(G.wake_btn, TR(L_OPT_WAKE_TIME), ROLE_SYSTEM_COMBOBOX, TR(L_ACC_WAKE_TIME));
    acc_time_btn(G.wake_btn, G.wake_h, G.wake_m);
    for (int i = 0; i < 7; i++) {
        day_name(i, true, v, 128);
        acc_control(G.day[i], v, ROLE_SYSTEM_CHECKBUTTON, TR(L_DAYS_TITLE));
        acc_checked(G.day[i], (G.rt.plan.days >> i) & 1);
    }
    static const struct { HWND *h; StrId n, d; bool *on; } T[] = {
        { &G.tg_remind, L_OPT_REMIND, L_ACC_REMIND, &G.remind_on },
        { &G.tg_force, L_OPT_FORCE, L_ACC_FORCE, &G.force_on },
        { &G.tg_auto, L_OPT_AUTO, L_ACC_AUTO, &G.auto_on },
        { &G.tg_tray, L_OPT_TRAY, L_ACC_TRAY, &G.tray_on },
        { &G.tg_wake, L_OPT_WAKE, L_ACC_WAKE, &G.wake_on },
    };
    for (int i = 0; i < 5; i++) {
        acc_control(*T[i].h, TR(T[i].n), ROLE_SYSTEM_CHECKBUTTON, TR(T[i].d));
        acc_checked(*T[i].h, *T[i].on);
    }
    wchar_t ht[128], hs[128];
    hero_strings(ht, 128, hs, 128);
    StringCchPrintfW(v, 128, TR(L_ACC_HERO), ht, hs);
    acc_desc(G.btn_start, v); /* name is the button text: Start / Cancel */
    acc_control(G.combo_lang, TR(L_LANGUAGE_TITLE), ROLE_SYSTEM_COMBOBOX, NULL);
    get_text(G.combo_lang, v, 128); acc_value(G.combo_lang, v);
    acc_control(G.rel_power, TR(L_LINK_POWER), ROLE_SYSTEM_LINK, TR(L_ACC_POWER));
    StringCchPrintfW(v, 128, TR(L_ACC_ABOUT), APP_NAME, APP_VERSION);
    acc_control(G.rel_about, TR(L_LINK_ABOUT), ROLE_SYSTEM_PUSHBUTTON, v);
}

static void acc_sync(HWND h) { (void)h; acc_sync_all(); } /* focus moved: refresh states */

/* the plan changed under us (read back from the task): redraw everything */
static void ui_after_plan_change(void) {
    if (!G.main || !G.ready) return;
    update_combo_texts();
    layout(G.main);
    update_views();
    RedrawWindow(G.main, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
    acc_sync_all();
}

/* The language setting changed (or its file did): load it and rebuild every text in place,
 * no restart. Popups are closed first; they are rebuilt with the new strings when reopened. */
static void apply_language(void) {
    bool was_rtl = lang_rtl();
    fly_close();
    tp_close();
    about_close();
    lang_load_saved();
    set_tray_tip();
    if (!G.main || !G.ready) return;
    if (lang_rtl() != was_rtl) { /* mirroring is fixed at creation (children don't re-mirror): rebuild */
        recreate_main();
        return;
    }
    /* fonts: Segoe UI serves most languages (Windows' font linking covers other scripts);
       Chinese, Japanese and Korean have their own face, so switching into or out of one rebuilds them */
    if (ui_font_face(lang_tag()) != g_ui_face) {
        free_ui_fonts();
        make_ui_fonts();
        for (HWND c = GetWindow(G.main, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
            SendMessageW(c, WM_SETFONT, (WPARAM)G.f_row, 0);
    }
    update_combo_texts();
    lang_caption_refresh(); /* the dropdown was closed above: its entries are rebuilt when it reopens */
    set_text(G.btn_start, TR(G.rt.active ? L_BTN_CANCEL : L_BTN_START));
    for (int i = 0; i < 7; i++) {
        wchar_t dn[32];
        day_name(i, false, dn, 32);
        set_text(G.day[i], dn);
    }
    layout(G.main);
    update_views();
    acc_sync_all();
    RedrawWindow(G.main, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

/* a control's drawn body (not its focus padding) in the window's client coordinates; mapping
   both corners at once keeps left < right in a mirrored window */
static RECT ctl_rect_in(HWND hwnd, HWND ctl) {
    RECT rc;
    GetWindowRect(ctl, &rc);
    MapWindowPoints(NULL, hwnd, (POINT *)&rc, 2);
    InflateRect(&rc, -S(CTL_PAD), -S(CTL_PAD));
    return rc;
}

/* WM_COMMAND: button clicks and flyout picks */
static LRESULT command(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = (int)(wp & 0xFFFF);
    int notif = (int)((wp >> 16) & 0xFFFF);
    /* a flyout pick arrives as WM_COMMAND with the item index in lp; a real click
     * carries the button hwnd */
    bool fly_pick = notif == 0 && FLY.pick >= 0 && !IsWindow((HWND)lp);
    if (fly_pick && id == ID_COMBO_ACTION) {
        G.rt.plan.task = (Task)FLY.pick;
        FLY.pick = -1;
        update_combo_texts();
        save_current();
        layout(hwnd); /* Wake up rows appear only for Sleep / Hibernate */
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    if (fly_pick && id == ID_COMBO_MODE) {
        G.rt.plan.mode = GUI_MODES[FLY.pick];
        FLY.pick = -1;
        update_combo_texts();
        layout(hwnd);       /* day row may appear/vanish; window resizes */
        save_current();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    if (fly_pick && id == ID_COMBO_CD) {
        G.rt.plan.cd = CDS[FLY.pick];
        FLY.pick = -1;
        update_combo_texts();
        save_current();
        return 0;
    }
    if (fly_pick && id == ID_COMBO_IDLE) {
        G.rt.plan.idle = IDLES[FLY.pick];
        FLY.pick = -1;
        update_combo_texts();
        save_current();
        return 0;
    }
    if (fly_pick && id == ID_COMBO_LANG) {
        int i = FLY.pick;
        FLY.pick = -1;
        if (i >= 0 && i < g_lang_n) {
            lang_save_setting(g_lang_items[i].tag); /* "" = Windows default */
            apply_language();
        }
        return 0;
    }
    if (notif == BN_CLICKED) {
        switch (id) {
        case ID_COMBO_LANG: {
            RECT brc = ctl_rect_in(hwnd, (HWND)lp);
            int sel = lang_picker_build(); /* listed now: files added or removed show up */
            FlyItem it[FLY_MAX];
            for (int i = 0; i < g_lang_n; i++) {
                it[i].label = g_lang_items[i].label;
                it[i].ic = (IconId)-1;
                it[i].color = 0;
            }
            fly_open(hwnd, id, it, g_lang_n, sel, &brc);
            return 0;
        }
        case ID_COMBO_ACTION: case ID_COMBO_MODE: case ID_COMBO_CD: case ID_COMBO_IDLE: {
            RECT brc = ctl_rect_in(hwnd, (HWND)lp);
            if (id == ID_COMBO_ACTION) {
                FlyItem it[NTASKS];
                for (int i = 0; i < NTASKS; i++) {
                    it[i].label = TASK_TR(i);
                    it[i].ic = TASK_ICON[i];
                    it[i].color = TASK_COLOR[i];
                }
                fly_open(hwnd, id, it, NTASKS, (int)G.rt.plan.task, &brc);
            } else if (id == ID_COMBO_MODE) {
                FlyItem it[NGUI_MODES];
                for (int i = 0; i < NGUI_MODES; i++) {
                    it[i].label = MODE_TR(i);
                    it[i].ic = (IconId)-1;
                    it[i].color = 0;
                }
                int sel = 0;
                for (int i = 0; i < NGUI_MODES; i++)
                    if (GUI_MODES[i] == G.rt.plan.mode) sel = i;
                fly_open(hwnd, id, it, NGUI_MODES, sel, &brc);
            } else if (id == ID_COMBO_CD) {
                FlyItem it[NCDS];
                static wchar_t lbl[NCDS][32];
                for (int i = 0; i < NCDS; i++) {
                    dur_label(CDS[i], lbl[i], 32);
                    it[i].label = lbl[i];
                    it[i].ic = (IconId)-1;
                    it[i].color = 0;
                }
                int sel = 0;
                for (int i = 0; i < NCDS; i++) if (CDS[i] == G.rt.plan.cd) sel = i;
                fly_open(hwnd, id, it, NCDS, sel, &brc);
            } else {
                FlyItem it[NIDLES];
                static wchar_t lbl[NIDLES][32];
                for (int i = 0; i < NIDLES; i++) {
                    dur_label(IDLES[i], lbl[i], 32);
                    it[i].label = lbl[i];
                    it[i].ic = (IconId)-1;
                    it[i].color = 0;
                }
                int sel = 0;
                for (int i = 0; i < NIDLES; i++) if (IDLES[i] == G.rt.plan.idle) sel = i;
                fly_open(hwnd, id, it, NIDLES, sel, &brc);
            }
            return 0;
        }
        case ID_TIME:
            tp_open(G.time_btn, 0, G.rt.plan.h, G.rt.plan.m);
            return 0;
        case ID_WAKE_TIME:
            tp_open(G.wake_btn, 1, G.wake_h, G.wake_m);
            return 0;
        case ID_START:
            toggle_start();
            return 0;
        case ID_TG_REMIND:
            G.remind_on = !G.remind_on;
            save_current();
            InvalidateRect(hwnd, &G.rc_remind, FALSE);
            return 0;
        case ID_TG_FORCE:
            G.force_on = !G.force_on;
            save_current();
            InvalidateRect(hwnd, &G.rc_force, FALSE);
            return 0;
        case ID_TG_AUTO:
            G.auto_on = !G.auto_on;
            set_startup(G.auto_on); /* not part of the schedule */
            InvalidateRect(hwnd, &G.rc_auto, FALSE);
            return 0;
        case ID_TG_TRAY: {
            wchar_t path[MAX_PATH];
            G.tray_on = !G.tray_on;
            ini_path(path, MAX_PATH);
            WritePrivateProfileStringW(L"PowerNapp", L"tray", G.tray_on ? L"1" : L"0", path);
            if (!G.tray_on && G.tray) DestroyWindow(G.tray);
            InvalidateRect(hwnd, &G.rc_tray, FALSE);
            return 0;
        }
        case ID_TG_WAKE:
            G.wake_on = !G.wake_on;
            G.wake_dirty = false;
            wake_save();
            if (G.wake_on) {
                if (!wts_wake_register(G.wake_h, G.wake_m)) {
                    G.wake_on = false;
                    wake_save();
                    MessageBoxW(hwnd, TR(L_ERR_WAKE), APP_NAME, MB_OK | MB_ICONWARNING | MB_DIR);
                }
            } else {
                wts_wake_delete();
            }
            layout(hwnd); /* the Wake time row appears or goes */
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case ID_REL_POWER:
            ShellExecuteW(NULL, L"open", L"ms-settings:powersleep", NULL, NULL, SW_SHOW);
            return 0;
        case ID_REL_ABOUT:
            about_open();
            return 0;
        default:
            if (id >= ID_DAY0 && id < ID_DAY0 + 7) {
                int i = id - ID_DAY0;
                G.rt.plan.days ^= 1 << i;
                InvalidateRect(hwnd, &G.rc_days, FALSE);
                save_current();
                return 0;
            }
            break;
        }
    }
    return 0;
}

/* the window's own drawing (cards, rows, labels); the controls draw themselves (draw_ctl) */
static void paint_main(HDC hdc, bool title) {
    Plan *p = &G.rt.plan;
    if (title) paint_title(hdc);
    /* hero */
    paint_card(hdc, &G.rc_hero);
    int hcy = (G.rc_hero.top + G.rc_hero.bottom) / 2;
    icon_bg(hdc, G.rc_hero.left + S(24), hcy - S(16), S(32), TASK_ICON[p->task], TASK_COLOR[p->task], T_CARD);
    wchar_t ht[128], hs[128];
    hero_strings(ht, 128, hs, 128);
    /* design: 20/28 title, 2 px gap, 14/20 subtitle, centered. A long title wraps to two lines;
       longer still (many days in a long language), it takes the 14 px heading font, up to three. */
    RECT htr = { G.rc_hero.left + S(24 + 32 + 20), 0, G.rc_hero.right - S(20 + 16) - g_bw, 0 };
    RECT m = { 0, 0, htr.right - htr.left, 0 };
    HFONT hf = G.f_hero;
    SelectObject(hdc, hf);
    DrawTextW(hdc, ht, -1, &m, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    if (m.bottom > S(56)) {
        SelectObject(hdc, hf = G.f_hdr);
        m.right = htr.right - htr.left;
        DrawTextW(hdc, ht, -1, &m, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    }
    int th = m.bottom < S(28) ? S(28) : m.bottom > S(60) ? S(60) : m.bottom;
    htr.top = hcy - (th + S(22)) / 2;
    htr.bottom = htr.top + th;
    text_at(hdc, ht, htr, hf, T_T1, DT_LEFT | DT_WORDBREAK);
    RECT hsr = { htr.left, htr.bottom + S(2), htr.right + S(8), htr.bottom + S(22) }; /* up to 8 px from Start */
    /* it carries the countdown: --check-fit counts an ellipsis here as a failure */
    g_fit_key = hero_sub_key();
    text_at(hdc, hs, hsr, G.f_row, T_T2, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    g_fit_key = -1;
    /* section headers: 14 px semibold, as in the design */
    text_at(hdc, TR(L_HDR_SCHEDULE), G.rc_sched_hdr, G.f_hdr, T_T1, DT_LEFT | DT_SINGLELINE);
    text_at(hdc, TR(L_HDR_TASK), G.rc_task_hdr, G.f_hdr, T_T1, DT_LEFT | DT_SINGLELINE);
    text_at(hdc, TR(L_HDR_OPTIONS), G.rc_opts_hdr, G.f_hdr, T_T1, DT_LEFT | DT_SINGLELINE);
    /* task rows */
    wchar_t sub[128];
    paint_row(hdc, &G.rc_action, TASK_ICON[p->task],
        TR(L_ACTION_TITLE), TR(L_ACTION_SUB), g_cw + S(16));
    paint_row(hdc, &G.rc_sched, IC_CALCLK, TR(L_SCHED_TITLE),
        TR(L_SCHED_SUB), g_cw + S(16));
    g_fit_key = time_row_sub(sub, 128);
    paint_row(hdc, &G.rc_time, IC_CLOCK, TR(L_TIME_TITLE), sub, g_sw + S(16));
    g_fit_key = -1;
    /* days row */
    if (p->mode == M_WEEKLY) {
        paint_card(hdc, &G.rc_days);
        wchar_t d[96];
        days_text(p->days, d, 96);
        int dcy = (G.rc_days.top + G.rc_days.bottom) / 2;
        RECT dr = { G.rc_days.left + S(18), dcy - S(18),
                    G.rc_days.right - S(16 + 6 * 4 + 18) - 7 * g_dw, dcy + S(2) };
        text_at(hdc, TR(L_DAYS_TITLE), dr, G.f_row, T_T1, DT_LEFT | DT_SINGLELINE);
        RECT dsr = { dr.left, dcy + S(2), dr.right, dcy + S(18) };
        StringCchPrintfW(sub, 128, TR(L_DAYS_SUB), d);
        g_fit_exempt = true; /* only sums up the day buttons beside it: may ellipsize */
        text_at(hdc, sub, dsr, G.f_sub, T_T2, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        g_fit_exempt = false;
    }
    /* options rows (On/Off labels left of the pill children) */
    static const struct { RECT *rc; IconId ic; StrId t; bool *on; } OPTS[] = {
        { &G.rc_remind, IC_ALERT, L_OPT_REMIND, &G.remind_on },
        { &G.rc_force, IC_APPS, L_OPT_FORCE, &G.force_on },
        { &G.rc_auto, IC_ROCKET, L_OPT_AUTO, &G.auto_on },
        { &G.rc_tray, IC_MONITOR, L_OPT_TRAY, &G.tray_on },
        { &G.rc_wake, IC_SUN, L_OPT_WAKE, &G.wake_on },
    };
    for (int i = 0; i < (wake_applies() ? 5 : 4); i++) { /* Wake up is last */
        paint_row(hdc, OPTS[i].rc, OPTS[i].ic, TR(OPTS[i].t), NULL, S(40 + 12 + 18) + g_ow); /* v3: single line */
        int ocy = (OPTS[i].rc->top + OPTS[i].rc->bottom) / 2;
        RECT lr = { OPTS[i].rc->right - S(16 + 40 + 12) - g_ow, ocy - S(10),
                    OPTS[i].rc->right - S(16 + 40 + 12), ocy + S(10) };
        text_at(hdc, TR(*OPTS[i].on ? L_OPT_ON : L_OPT_OFF), lr, G.f_row, T_T1,
            DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    }
    if (wake_applies() && G.wake_on) {
        paint_row(hdc, &G.rc_wake_time, IC_CLOCK, TR(L_OPT_WAKE_TIME), NULL, g_sw + S(16));
    }
    paint_row(hdc, &G.rc_lang, IC_LANG, TR(L_LANGUAGE_TITLE), NULL, g_lw + S(16));
    /* footer links are owner-drawn buttons (see WM_DRAWITEM) */
}

/* an owner-drawn control, into hdc in its own client coordinates */
static void draw_ctl(HDC hdc, HWND h, int id) {
    if (id == ID_COMBO_ACTION || id == ID_COMBO_MODE || id == ID_COMBO_CD || id == ID_COMBO_IDLE ||
            id == ID_COMBO_LANG)
        paint_combo_btn(hdc, h);
    else if (id == ID_TIME) {
        paint_time_btn(hdc, h, G.rt.plan.h, G.rt.plan.m);
    } else if (id == ID_WAKE_TIME) {
        paint_time_btn(hdc, h, G.wake_h, G.wake_m);
    } else if (id >= ID_DAY0 && id < ID_DAY0 + 7) {
        paint_day_btn(hdc, h, (G.rt.plan.days >> (id - ID_DAY0)) & 1);
    } else if (id == ID_TG_REMIND)
        paint_toggle(hdc, h, G.remind_on);
    else if (id == ID_TG_FORCE)
        paint_toggle(hdc, h, G.force_on);
    else if (id == ID_TG_AUTO)
        paint_toggle(hdc, h, G.auto_on);
    else if (id == ID_TG_TRAY)
        paint_toggle(hdc, h, G.tray_on);
    else if (id == ID_TG_WAKE)
        paint_toggle(hdc, h, G.wake_on);
    else if (id == ID_START)
        paint_start_btn(hdc, h, G.rt.active);
    else if (id == ID_REL_POWER)
        paint_link(hdc, h, TR(L_LINK_POWER));
    else if (id == ID_REL_ABOUT)
        paint_link(hdc, h, TR(L_LINK_ABOUT));
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        G.main = hwnd;
        g_dpi = GetDpiForWindow(hwnd);
        { /* testing aid: render at another scale, e.g. POWERNAPP_TEST_DPI=144 for 150% */
            wchar_t t[8];
            if (GetEnvironmentVariableW(L"POWERNAPP_TEST_DPI", t, 8)) g_dpi = _wtoi(t) >= 96 ? _wtoi(t) : g_dpi;
        }
        theme_dpi_reset();
        make_ui_fonts();
        acc_init();
        DWORD obtn = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW; /* Tab-navigable */

        /* created in reading order: that is the Tab order */
        G.btn_start = mk_child(hwnd, L"BUTTON", TR(L_BTN_START), obtn, ID_START, G.f_row);
        /* combo texts are set by update_combo_texts() below */
        G.combo_action = mk_child(hwnd, L"BUTTON", L"", obtn, ID_COMBO_ACTION, G.f_row);
        G.combo_mode = mk_child(hwnd, L"BUTTON", L"", obtn, ID_COMBO_MODE, G.f_row);
        G.combo_cd = mk_child(hwnd, L"BUTTON", L"", obtn, ID_COMBO_CD, G.f_row);
        G.combo_idle = mk_child(hwnd, L"BUTTON", L"", obtn, ID_COMBO_IDLE, G.f_row);
        G.time_btn = mk_child(hwnd, L"BUTTON", L"", obtn, ID_TIME, G.f_row);
        for (int k = 0; k < 7; k++) { /* G.day[] and the ids are Monday-based; created from the first day */
            int i = (first_day() + k) % 7;
            wchar_t dn[32];
            day_name(i, false, dn, 32);
            G.day[i] = mk_child(hwnd, L"BUTTON", dn, obtn, ID_DAY0 + i, G.f_row);
        }
        G.tg_remind = mk_child(hwnd, L"BUTTON", L"", obtn, ID_TG_REMIND, G.f_row);
        G.tg_force = mk_child(hwnd, L"BUTTON", L"", obtn, ID_TG_FORCE, G.f_row);
        G.tg_auto = mk_child(hwnd, L"BUTTON", L"", obtn, ID_TG_AUTO, G.f_row);
        G.tg_tray = mk_child(hwnd, L"BUTTON", L"", obtn, ID_TG_TRAY, G.f_row);
        G.tg_wake = mk_child(hwnd, L"BUTTON", L"", obtn, ID_TG_WAKE, G.f_row);
        G.wake_btn = mk_child(hwnd, L"BUTTON", L"", obtn, ID_WAKE_TIME, G.f_row);
        G.combo_lang = mk_child(hwnd, L"BUTTON", L"", obtn, ID_COMBO_LANG, G.f_row);
        G.rel_power = mk_child(hwnd, L"BUTTON", L"", obtn, ID_REL_POWER, G.f_row);
        G.rel_about = mk_child(hwnd, L"BUTTON", L"", obtn, ID_REL_ABOUT, G.f_row);

        G.ready = true;
        update_combo_texts();
        lang_caption_refresh();
        layout(hwnd);
        update_views();
        acc_sync_all();
        return 0;
    }
    case WM_DPICHANGED: { /* moved to a monitor with other scaling: rebuild at the new DPI */
        const RECT *sug = (const RECT *)lp;
        g_dpi = HIWORD(wp);
        theme_dpi_reset();
        free_ui_fonts();
        make_ui_fonts();
        fly_close();
        tp_close();
        SetWindowPos(hwnd, NULL, sug->left, sug->top, sug->right - sug->left, sug->bottom - sug->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        layout(hwnd);
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        /* Drawn off-screen into a bitmap the size of the dirty rect (viewport-shifted
         * so coordinates stay in client space), clipped to it, then blitted once:
         * no flicker, and the anti-aliasing only runs over the dirty pixels. */
        PAINTSTRUCT ps;
        HDC wdc = BeginPaint(hwnd, &ps);
        RECT dirty = ps.rcPaint;
        int dw = dirty.right - dirty.left, dh = dirty.bottom - dirty.top;
        if (dw <= 0 || dh <= 0) { EndPaint(hwnd, &ps); return 0; }
        HDC hdc = CreateCompatibleDC(wdc);
        HBITMAP buf = CreateCompatibleBitmap(wdc, dw, dh);
        HGDIOBJ obuf = SelectObject(hdc, buf);
        dc_match(hdc, wdc);
        SetViewportOrgEx(hdc, -dirty.left, -dirty.top, NULL);
        IntersectClipRect(hdc, dirty.left, dirty.top, dirty.right, dirty.bottom);
        RECT crc;
        GetClientRect(hwnd, &crc);
        fill_rect_c(hdc, &dirty, T_WIN);
        if (G.ready) paint_main(hdc, dirty.top < S(TITLE_H));
        SetViewportOrgEx(hdc, 0, 0, NULL);
        BitBlt(wdc, dirty.left, dirty.top, dw, dh, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, obuf);
        DeleteObject(buf);
        DeleteDC(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, T_CTL);
        SetTextColor(hdc, T_T1);
        if (!G.br_ctl) G.br_ctl = CreateSolidBrush(T_CTL);
        return (LRESULT)G.br_ctl;
    }
    case WM_DRAWITEM: {
        if (!G.ready) return 0;
        /* painted into a bitmap and blitted once, so a repaint never shows half-drawn */
        const DRAWITEMSTRUCT *di = (const DRAWITEMSTRUCT *)lp;
        HDC screen = di->hDC;
        int bw = di->rcItem.right - di->rcItem.left, bh = di->rcItem.bottom - di->rcItem.top;
        HDC mem = CreateCompatibleDC(screen);
        HBITMAP bmp = CreateCompatibleBitmap(screen, bw, bh);
        HGDIOBJ obmp = SelectObject(mem, bmp);
        dc_match(mem, screen);
        SetViewportOrgEx(mem, -di->rcItem.left, -di->rcItem.top, NULL);
        /* focus ring only when focus came from the keyboard */
        g_focus_cues = (di->itemState & ODS_FOCUS) && G.kbd_nav;
        draw_ctl(mem, di->hwndItem, (int)di->CtlID);
        SetViewportOrgEx(mem, 0, 0, NULL);
        BitBlt(screen, di->rcItem.left, di->rcItem.top, bw, bh, mem, 0, 0, SRCCOPY);
        SelectObject(mem, obmp);
        DeleteObject(bmp);
        DeleteDC(mem);
        return 0;
    }
    case WM_COMMAND: {
        if (!G.ready) return 0;
        LRESULT r = command(hwnd, wp, lp);
        acc_sync_all(); /* names, values and checked states follow every change */
        return r;
    }
    case WM_MOUSEWHEEL:
        if (TP.wnd) { SendMessageW(TP.wnd, WM_MOUSEWHEEL, wp, lp); return 0; }
        if (FLY.wnd) { fly_wheel(GET_WHEEL_DELTA_WPARAM(wp)); return 0; }
        break;
    case WM_NCCALCSIZE:
        if (wp) { /* drop the standard caption: client starts at the window top */
            NCCALCSIZE_PARAMS *pp = (NCCALCSIZE_PARAMS *)lp;
            LONG top = pp->rgrc[0].top;
            LRESULT r = DefWindowProcW(hwnd, msg, wp, lp);
            pp->rgrc[0].top = top;
            return r;
        }
        break;
    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProcW(hwnd, msg, wp, lp);
        if (hit == HTCLIENT) {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            int b = cap_btn_at(pt); /* client x is mirrored in RTL: the buttons are then on the left */
            if (pt.y < S(TITLE_H)) return b ? CAP_HT[b] : HTCAPTION;
        }
        return hit;
    }
    /* the caption buttons are ours: hover and press them here, not with the system's frame code */
    case WM_NCMOUSEMOVE: {
        int b = cap_btn_of_ht(wp);
        if (b != G.cap_hover) { G.cap_hover = b; invalidate_title(); }
        if (!b) break;
        set_hover_row(NULL);
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE | TME_NONCLIENT, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_NCMOUSELEAVE:
        if (G.cap_hover && !G.cap_down) { G.cap_hover = 0; invalidate_title(); }
        break;
    case WM_NCLBUTTONDOWN: case WM_NCLBUTTONDBLCLK: {
        int b = cap_btn_of_ht(wp);
        if (!b) break;
        G.cap_down = b; G.cap_hover = b; SetCapture(hwnd); invalidate_title(); /* up: WM_LBUTTONUP */
        return 0;
    }
    case WM_NCLBUTTONUP:
        if (cap_btn_of_ht(wp)) return 0;
        break;
    case WM_ACTIVATE:
        G.inactive = LOWORD(wp) == WA_INACTIVE;
        invalidate_title();
        if (G.inactive) { /* remember the focused control, as dialogs do */
            HWND f = GetFocus();
            if (f && IsChild(hwnd, f)) G.last_focus = f;
            return 0;
        }
        if (G.ready) refresh_from_task(); /* edited in Task Scheduler meanwhile? */
        if (G.last_focus && IsWindow(G.last_focus) && IsWindowVisible(G.last_focus)) {
            SetFocus(G.last_focus); /* back where the keyboard was, not the window itself */
            return 0;
        }
        break;
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int h = cap_btn_at(pt);
        if (h != G.cap_hover) {
            G.cap_hover = h;
            invalidate_title();
        }
        set_hover_row(row_at(pt));
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (G.cap_hover && !G.cap_down) { /* maybe left onto a caption button (non-client) */
            POINT sp, pt;
            GetCursorPos(&sp);
            pt = sp;
            ScreenToClient(hwnd, &pt);
            int b = WindowFromPoint(sp) == hwnd ? cap_btn_at(pt) : 0;
            if (b != G.cap_hover) { G.cap_hover = b; invalidate_title(); }
        }
        hover_from_cursor(); /* left onto a toggle: its row stays hot */
        return 0;
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int b = cap_btn_at(pt);
        if (b) { G.cap_down = b; SetCapture(hwnd); invalidate_title(); }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (!G.cap_down) return 0;
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int b = G.cap_down;
        G.cap_down = 0;
        ReleaseCapture();
        G.cap_hover = cap_btn_at(pt);
        invalidate_title();
        if (cap_btn_at(pt) == b) {
            if (b == 1) ShowWindow(hwnd, SW_MINIMIZE);
            else if (b == 3) PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
        return 0;
    }
    case WM_SETTINGCHANGE:
        if (lp && lstrcmpiW((const wchar_t *)lp, L"intl") == 0) { /* regional format: clock, first day */
            tp_close();
            layout(hwnd);
            acc_sync_all();
            RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            return 0;
        }
        read_theme();
        apply_dark_mode(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_CLOSE: {
        /* The window is always destroyed, not hidden: reopening rebuilds it, and the
         * process left behind is just the tray icon -- or nothing at all. */
        wake_flush();
        bool in_process = G.rt.active && !G.delegated; /* idle mode (or a refused task) needs us */
        if (G.tray_on && !in_process) {
            /* Restart as a fresh "--tray" process: the UI DLLs and their heaps loaded by
             * the window can't be unloaded, a new process never maps them. Windows go
             * first so the new copy's single-instance check doesn't find this one. */
            wchar_t exe[MAX_PATH], cmd[MAX_PATH + 16];
            STARTUPINFOW si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            GetModuleFileNameW(NULL, exe, MAX_PATH);
            StringCchPrintfW(cmd, MAX_PATH + 16, L"\"%s\" --tray", exe);
            if (G.tray) DestroyWindow(G.tray);
            DestroyWindow(hwnd); /* no tray window left: posts WM_QUIT */
            if (CreateProcessW(exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
            return 0;
        }
        if (G.tray_on || in_process) ensure_tray();
        DestroyWindow(hwnd); /* quits the app if no tray icon is left */
        set_tray_tip();
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
        return 0;
    }
    case WM_DESTROY: {
        fly_close();
        tp_close();
        about_close();
        G.hover_btn = NULL;
        G.hover_row = NULL;
        G.last_focus = NULL;
        G.ready = false;
        G.main = NULL;
        free_ui_fonts();
        if (G.br_ctl) { DeleteObject(G.br_ctl); G.br_ctl = NULL; }
        if (!G.tray && !g_recreating) PostQuitMessage(0);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ------------------------------------------------------------------ run GUI */

/* The main window is built on demand and destroyed on close. The comctl32/uxtheme/
 * dwmapi/shell32/powrprof imports are delay-loaded (build.bat), so a tray-only process
 * maps only what one hidden window and one notify icon need. */
static HWND create_main(int x, int y);

static void show_main(void) {
    if (G.main) {
        ShowWindow(G.main, SW_SHOWNORMAL);
        SetForegroundWindow(G.main);
        return;
    }
    create_main(CW_USEDEFAULT, CW_USEDEFAULT);
}

/* The language switched between left-to-right and right-to-left: build the window again,
 * mirrored or not, where it was, with the keyboard back on the Language control. */
static void recreate_main(void) {
    RECT wr;
    GetWindowRect(G.main, &wr);
    bool kbd = GetFocus() == G.combo_lang;
    g_recreating = true;
    DestroyWindow(G.main);
    g_recreating = false;
    G.cap_hover = G.cap_down = 0;
    if (!create_main(wr.left, wr.top)) { if (!G.tray) PostQuitMessage(0); return; }
    if (kbd) SetFocus(G.combo_lang);
}

static HWND create_main(int x, int y) {
    static bool cc_ready;
    if (!cc_ready) {
        INITCOMMONCONTROLSEX icc;
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_PROGRESS_CLASS;
        InitCommonControlsEx(&icc);
        cc_ready = true;
    }
    read_theme();
    theme_tokens();
    /* estimate size: layout needs a window; create then resize in WM_CREATE */
    RECT adj = { 0, 0, 640, 700 };
    AdjustWindowRect(&adj, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    HWND hwnd = CreateWindowExW(wnd_ex_dir(), L"PowerNappWnd", APP_NAME, /* mirrored for RTL */
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        x, y, adj.right - adj.left, adj.bottom - adj.top,
        NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!hwnd) return NULL;
    apply_dark_mode(hwnd);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    layout(hwnd); /* height from the custom frame */
    keep_on_screen(hwnd); /* placed for the first estimate, now taller */
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    return hwnd;
}

static UINT g_taskbar_created;

static LRESULT CALLBACK tray_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TRAY:
        if ((lp & 0xFFFF) == WM_LBUTTONUP) show_main();
        else if ((lp & 0xFFFF) == WM_RBUTTONUP) show_tray_menu(hwnd);
        return 0;
    case WM_COMMAND:
        switch (wp & 0xFFFF) {
        case IDM_SHOW: show_main(); return 0;
        case IDM_START: toggle_start(); set_tray_tip(); return 0;
        case IDM_NOW: do_task(T_SLEEP); return 0;
        case IDM_EXIT:
            if (G.main) DestroyWindow(G.main);
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_DESTROY:
        del_tray(hwnd);
        G.tray = NULL;
        if (!G.main) PostQuitMessage(0);
        return 0;
    default:
        if (msg == g_taskbar_created && msg) { add_tray(hwnd); set_tray_tip(); return 0; } /* Explorer restarted */
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void ensure_tray(void) {
    if (G.tray) return;
    G.tray = CreateWindowExW(0, L"PowerNappTray", L"", WS_POPUP, 0, 0, 0, 0,
        NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (G.tray) { add_tray(G.tray); set_tray_tip(); }
}

static void CALLBACK timer_proc(HWND h, UINT m, UINT_PTR id, DWORD t) {
    (void)h; (void)m; (void)id; (void)t;
    if (G.rt.active) tick();
}

static void run_gui(bool tray) {
    { /* single instance: hand the request to a running copy */
        HWND other = FindWindowW(L"PowerNappWnd", NULL);
        if (other) {
            if (!tray) { ShowWindow(other, SW_SHOWNORMAL); SetForegroundWindow(other); }
            return;
        }
        other = FindWindowW(L"PowerNappTray", NULL);
        if (other) {
            if (!tray) { AllowSetForegroundWindow(ASFW_ANY); PostMessageW(other, WM_COMMAND, IDM_SHOW, 0); }
            return;
        }
    }
    HINSTANCE hi = GetModuleHandleW(NULL);
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1)); /* small title-bar size is picked from the same resource */
    wc.lpszClassName = L"PowerNappWnd";
    RegisterClassW(&wc);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = fly_proc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = FLY_CLASS;
    RegisterClassW(&wc);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = tp_proc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = TP_CLASS;
    RegisterClassW(&wc);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = about_proc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = ABOUT_CLASS;
    RegisterClassW(&wc);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = tray_proc;
    wc.hInstance = hi;
    wc.lpszClassName = L"PowerNappTray";
    RegisterClassW(&wc);
    g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

    memset(&G.rt, 0, sizeof(G.rt));
    load_config(&G.rt.plan);
    G.remind_on = G.rt.plan.remind_min > 0;
    G.force_on = G.rt.plan.force;
    G.auto_on = get_startup();
    {
        wchar_t path[MAX_PATH];
        ini_path(path, MAX_PATH);
        G.tray_on = GetPrivateProfileIntW(L"PowerNapp", L"tray", 1, path) != 0;
        G.wake_on = GetPrivateProfileIntW(L"PowerNapp", L"wake", 0, path) != 0;
        G.wake_h = GetPrivateProfileIntW(L"PowerNapp", L"wake_h", 7, path) % 24;
        G.wake_m = GetPrivateProfileIntW(L"PowerNapp", L"wake_m", 0, path) % 60;
    }
    { /* the "PowerNapp" task is the schedule: show what it says */
        Runtime r = G.rt;
        if (ts_load_plan(&r)) {
            G.rt = r;
            G.delegated = true;
            G.remind_on = r.plan.remind_min > 0;
            G.force_on = r.plan.force;
        }
    }
    SetTimer(NULL, 0, 1000, timer_proc);

    if (!tray) show_main();
    else if (G.tray_on) ensure_tray();
    else return; /* tray icon turned off: nothing to show, Task Scheduler has the job */
    if (!G.main && !G.tray) return;
    if (tray) SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        /* dialog-style keyboard navigation for the main window: Tab / Shift+Tab move
         * between controls, Space / Enter press them */
        /* focus ring only while navigating by keyboard: a key shows it, a click hides it
         * (Windows' own cue stays on for good after the first key press) */
        bool key = msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN;
        bool click = msg.message == WM_LBUTTONDOWN || msg.message == WM_RBUTTONDOWN ||
                     msg.message == WM_NCLBUTTONDOWN;
        if ((key && !G.kbd_nav) || (click && G.kbd_nav)) {
            G.kbd_nav = key;
            HWND f = GetFocus();
            if (f && G.main && IsChild(G.main, f)) InvalidateRect(f, NULL, FALSE);
        }
        if (G.main && IsDialogMessageW(G.main, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

/* ------------------------------------------------------------------ --check-fit
 * Lays out and paints every state of the window, every dropdown's items, About and the
 * reminder card into a memory bitmap, and reports each text wider than its box (theme.c).
 * Pure offscreen: the window is never shown and has its own class (a running copy never
 * finds it); no settings, scheduled tasks, startup entry or tray icon are read or written. */
static int check_fit(void) {
    HINSTANCE hi = GetModuleHandleW(NULL);
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hi;
    wc.lpszClassName = L"PowerNappFit";
    RegisterClassW(&wc);
    g_fit = true;
    g_clock_test = L"h:mm tt"; /* 12-hour: the AM/PM column is the tight one */
    read_theme();
    theme_tokens();
    Plan *p = &G.rt.plan;
    p->h = 23; p->cd = 90; p->idle = 15;
    p->days = 0x3F; /* six days: the longest list */
    G.wake_h = 7;
    HWND hwnd = CreateWindowExW(wnd_ex_dir(), L"PowerNappFit", APP_NAME,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        -32000, -32000, 640, 700, NULL, NULL, hi, NULL); /* hidden: no WS_VISIBLE, never shown */
    if (!hwnd) return -1;
    { /* worst cases instead of the clock, so the result doesn't depend on when it runs: the widest
         time (minute 59, the hour and AM/PM marker that measure widest), the widest day word
         (Today, Tomorrow or a weekday), the widest countdown and idle choices */
        static wchar_t day[32];
        wchar_t t[96];
        int best = -1;
        for (int h = 0; h < 24; h++) {
            fmt_time(h, 59, t, 96);
            int w = text_px(G.f_row, t);
            if (w > best) { best = w; g_fit_h = h; }
        }
        g_fit_m = 59;
        p->h = G.wake_h = g_fit_h;
        p->m = G.wake_m = 59;
        best = -1;
        for (int i = 0; i < 9; i++) {
            if (i < 7) day_name(i, true, t, 96);
            else StringCchCopyW(t, 96, TR(i == 7 ? L_DAY_TODAY : L_DAY_TOMORROW));
            int w = text_px(G.f_row, t);
            if (w > best) { best = w; StringCchCopyW(day, 32, t); }
        }
        g_fit_day = day;
        best = -1;
        for (int i = 0; i < NCDS; i++) {
            dur_label(CDS[i], t, 96);
            int w = text_px(G.f_row, t);
            if (w > best) { best = w; p->cd = CDS[i]; }
        }
        best = -1;
        for (int i = 0; i < NIDLES; i++) {
            dur_label(IDLES[i], t, 96);
            int w = text_px(G.f_row, t);
            if (w > best) { best = w; p->idle = IDLES[i]; }
        }
    }
    HDC sdc = GetDC(NULL), hdc = CreateCompatibleDC(sdc);
    HBITMAP bm = CreateCompatibleBitmap(sdc, S(PAGE_W), S(1200));
    ReleaseDC(NULL, sdc);
    HGDIOBJ ob = SelectObject(hdc, bm);
    for (int st = 0; st < NTASKS * NGUI_MODES * 4; st++) { /* task x mode x Start/Cancel x options off/on */
        int k = st / (NTASKS * NGUI_MODES);
        p->task = (Task)(st % NTASKS);
        p->mode = GUI_MODES[st / NTASKS % NGUI_MODES];
        G.rt.active = k & 1;
        G.rt.fire_at = local_now_secs() + 5400;
        /* time left at its widest: almost a day (daily), almost a week (weekly), the longest countdown */
        g_fit_rem = p->mode == M_WEEKLY ? 7 * 86400LL - 1 : p->mode == M_COUNTDOWN ? CDS[NCDS - 1] * 60LL - 1 : 86400LL - 1;
        G.remind_on = G.force_on = G.auto_on = G.tray_on = G.wake_on = k >> 1;
        update_combo_texts();
        layout(hwnd);
        update_views();
        paint_main(hdc, true);
        for (HWND c = GetWindow(hwnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
            if (GetWindowLongW(c, GWL_STYLE) & WS_VISIBLE) draw_ctl(hdc, c, GetDlgCtrlID(c));
    }
    if (!_wcsicmp(lang_tag(), L"en") && (g_cw != S(200) || g_lw != S(200) || g_sw != S(242) ||
            g_dw != S(44) || g_bw != S(120) || g_ow != S(34))) { /* English keeps the design exactly */
        wprintf(L"English: not the design widths (combo %d, language %d, time %d, day %d, start %d, on/off %d)\n",
            g_cw, g_lw, g_sw, g_dw, g_bw, g_ow);
        g_fit_n++;
    }
    /* every dropdown, opened from its control */
    static wchar_t lbl[NCDS + NIDLES][32];
    HWND owner[5] = { G.combo_action, G.combo_mode, G.combo_cd, G.combo_idle, G.combo_lang };
    for (int f = 0; f < 5; f++) {
        int n = f == 0 ? NTASKS : f == 1 ? NGUI_MODES : f == 2 ? NCDS : f == 3 ? NIDLES : 0;
        if (f == 4) { lang_picker_build(); n = g_lang_n; }
        for (int i = 0; i < n; i++) {
            const wchar_t *t;
            if (f == 0) t = TASK_TR(i);
            else if (f == 1) t = MODE_TR(i);
            else if (f == 4) t = g_lang_items[i].label;
            else {
                wchar_t *d = lbl[(f == 3 ? NCDS : 0) + i];
                dur_label(f == 2 ? CDS[i] : IDLES[i], d, 32);
                t = d;
            }
            FLY.items[i].label = t;
            FLY.items[i].ic = (IconId)-1;
        }
        RECT br = ctl_rect_in(hwnd, owner[f]);
        FLY.count = FLY.rows = n;
        FLY.top = 0; FLY.sel = 0; FLY.hover = -1;
        RECT rc = { 0, 0, fly_width(&br, FLY.items, n), n * S(36) + S(4) };
        fly_paint(hdc, &rc);
    }
    ab_measure();
    about_paint(hdc);
    RM.f_title = make_font(ui_font_face(lang_tag()), L"Segoe UI", -20, FW_SEMIBOLD);
    RM.f_text = make_font(ui_font_face(lang_tag()), L"Segoe UI", -14, FW_NORMAL);
    for (int t = 0; t < NTASKS; t++) {
        RM.task = (Task)t;
        rm_measure();
        rm_paint(hdc, 60, 60000);
    }
    fit_flush();
    SelectObject(hdc, ob);
    DeleteObject(bm);
    DeleteDC(hdc);
    DestroyWindow(hwnd);
    return g_fit_n;
}
