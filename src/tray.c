/* PowerNapp: notification-area icon. Part of the unity build: included by powernapp.c, in order. */

/* ------------------------------------------------------------------ tray */

static void add_tray(HWND hwnd) {
    NOTIFYICONDATAW nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0); /* powernapp.rc */
    wcscpy_s(nid.szTip, 128, APP_NAME);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

static void del_tray(HWND hwnd) {
    NOTIFYICONDATAW nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

static void set_tray_tip(void) {
    if (!G.tray) return;
    NOTIFYICONDATAW nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = G.tray;
    nid.uID = 1;
    nid.uFlags = NIF_TIP;
    wchar_t d[96] = L"";
    if (G.rt.active) rt_describe(&G.rt, d, 96);
    StringCchPrintfW(nid.szTip, 128, APP_NAME L"%ls%ls%ls%ls", d[0] ? L"\n" : L"", d,
        G.delegated ? L"\n" : L"", G.delegated ? TR(L_TRAY_DELEGATED) : L"");
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static void show_tray_menu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_SHOW, TR(L_TRAY_SHOW));
    AppendMenuW(menu, MF_STRING, IDM_START, TR(G.rt.active ? L_TRAY_CANCEL : L_TRAY_START));
    AppendMenuW(menu, MF_STRING, IDM_NOW, TR(L_TRAY_SLEEP_NOW));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, TR(G.delegated ? L_TRAY_EXIT_KEEP : L_TRAY_EXIT));
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | (lang_rtl() ? TPM_LAYOUTRTL : 0), pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);
}

