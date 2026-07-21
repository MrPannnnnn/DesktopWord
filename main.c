#include "main.h"
#include "bg_image.h"

/* --- Global state --- */
HINSTANCE g_hInst       = NULL;
HWND      g_hWndMain    = NULL;
HWND      g_hWndWord    = NULL;
BOOL      g_desktop_visible = TRUE;
BOOL      g_app_running = FALSE;

/* currently displayed words and their positions */
WordEntry* g_displayed[MAX_DISPLAY_COUNT] = {NULL};
int        g_displayed_count  = 0;
int        g_word_pos_y[MAX_DISPLAY_COUNT] = {0};
int        g_word_pos_h[MAX_DISPLAY_COUNT] = {0};
int        g_hovered_index    = -1;
BOOL       g_timer_paused     = FALSE;
HWND       g_hWndWordMgr      = NULL;

/* Forward declarations */
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK WordWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static void RegisterClasses(void);
static BOOL CreateOverlayWindow(void);
static void RefreshWordDisplay(void);
static void DoImportWords(HWND hwnd);
static void ShowStatsDialog(HWND hwndParent);
/* ======================================================================== */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int nShow) {
    g_hInst = hInst;

    /* Single instance check */
    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"DesktopWord_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"桌面单词已在运行中。", APP_TITLE, MB_ICONINFORMATION);
        return 0;
    }

    /* Initialize common controls */
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    /* Initialize GDI+ (via bg_image helper) */
    if (!BgInitGdiPlus()) {
        MessageBoxW(NULL, L"GDI+ 初始化失败。", APP_TITLE, MB_ICONERROR);
    }

    /* Load settings */
    LoadSettings();

    /* Load background image if configured */
    if (g_bg_type == BG_IMAGE && g_bg_path[0]) {
        if (!BgLoadImage(g_bg_path)) {
            /* Silently fall back to transparent if image fails to load */
            g_bg_type = BG_TRANSPARENT;
        }
    }

    /* Register window classes */
    RegisterClasses();

    /* Create hidden main window (message handler) */
    g_hWndMain = CreateWindowExW(0, L"DesktopWordMain", APP_TITLE,
                                  WS_OVERLAPPED, 0, 0, 0, 0,
                                  NULL, NULL, hInst, NULL);
    if (!g_hWndMain) {
        MessageBoxW(NULL, L"创建主窗口失败。", APP_TITLE, MB_ICONERROR);
        return 1;
    }

    /* Load word list */
    wchar_t wordPath[MAX_PATH];
    GetExePath(wordPath, MAX_PATH);
    wcscat(wordPath, L"\\words.txt");
    int loaded = LoadWords(wordPath);
    if (loaded == 0) {
        MessageBoxW(NULL, L"未找到词库文件 words.txt\n\n"
                    L"请将 words.txt 放在程序同目录下。",
                    APP_TITLE, MB_ICONERROR);
    }

    /* Load progress from exe directory */
    wchar_t progPath[MAX_PATH];
    GetExePath(progPath, MAX_PATH);
    wcscat(progPath, L"\\progress.dat");
    LoadProgress(progPath);

    /* Create overlay window (desktop-embedded word display) */
    if (!CreateOverlayWindow()) {
        MessageBoxW(NULL, L"创建桌面覆盖窗口失败。", APP_TITLE, MB_ICONERROR);
    }

    /* Create system tray icon */
    CreateTrayIcon(g_hWndMain);

    /* Word refresh timer */
    SetTimer(g_hWndMain, ID_TIMER_REFRESH, g_refresh_sec * 1000, NULL);

    /* Initial word display + keep window at bottom */
    SetWindowPos(g_hWndWord, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    RefreshWordDisplay();

    /* Update tray stats */
    UpdateTrayStats();

    /* Diagnostics: show word count */
    if (g_word_count == 0) {
        MessageBoxW(NULL, L"未能加载任何单词，请检查 words.txt。"
                    L"\n程序将继续运行，您可以通过托盘菜单导入词库。",
                    APP_TITLE, MB_ICONWARNING);
    }

    g_app_running = TRUE;

    /* Message loop */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    /* Cleanup */
    KillTimer(g_hWndMain, ID_TIMER_REFRESH);
    RemoveTrayIcon();

    /* Save familiar words + learning progress */
    {
        wchar_t famPath[MAX_PATH];
        GetFamiliarPath(famPath, MAX_PATH);
        SaveFamiliarWords(famPath);
    }
    SaveProgressDefault();
    SaveSettings();

    if (g_words) free(g_words);

    BgShutdownGdiPlus();
    CloseHandle(hMutex);

    return (int)msg.wParam;
}

/* ========================================================================
 * Window class registration
 * ======================================================================== */

static void RegisterClasses(void) {
    /* Main hidden window */
    WNDCLASSW wcMain = {0};
    wcMain.lpfnWndProc   = MainWndProc;
    wcMain.hInstance     = g_hInst;
    wcMain.lpszClassName = L"DesktopWordMain";
    RegisterClassW(&wcMain);

    /* Word overlay window */
    WNDCLASSW wcWord = {0};
    wcWord.style         = CS_DBLCLKS;  /* needed for WM_LBUTTONDBLCLK */
    wcWord.lpfnWndProc   = WordWndProc;
    wcWord.hInstance     = g_hInst;
    wcWord.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wcWord.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcWord.lpszClassName = L"DesktopWordOverlay";
    RegisterClassW(&wcWord);
}

/* ========================================================================
 * Overlay window (the visible word display on desktop)
 * ======================================================================== */

static BOOL CreateOverlayWindow(void) {
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    /* Note: WS_EX_TRANSPARENT removed to allow double-click interaction */
    DWORD style = WS_POPUP;

    g_hWndWord = CreateWindowExW(exStyle, L"DesktopWordOverlay", L"",
                                  style, 0, 0, 100, 100,
                                  NULL, NULL, g_hInst, NULL);
    if (!g_hWndWord) {
        MessageBoxW(NULL, L"创建桌面覆盖窗口失败。", APP_TITLE, MB_ICONERROR);
        return FALSE;
    }

    /* Position at top-right quarter of screen, keep at bottom Z-order */
    PositionOverlayWindow(g_hWndWord);
    ShowWindow(g_hWndWord, SW_SHOWNOACTIVATE);

    return TRUE;
}

/* ========================================================================
 * Word refresh logic
 * ======================================================================== */

static void RefreshWordDisplay(void) {
    if (!g_hWndWord || !g_words || g_word_count == 0) return;

    g_hovered_index = -1;  /* stale hover data — reset before new render */

    /* Pick words */
    WordEntry* selected[MAX_DISPLAY_COUNT] = {NULL};
    int n = PickWords(selected, g_display_count);

    if (n == 0) return;

    /* Update progress for each selected word */
    for (int i = 0; i < n; i++) {
        WordEntry* e = selected[i];
        int today = TodayDays();

        if (e->prog.first_seen == 0) {
            e->prog.first_seen = today;
            e->prog.review_count = 1;
            e->prog.next_review = today + EB_INTERVALS[1];
        } else {
            e->prog.review_count++;
            int idx = e->prog.review_count;
            if (idx >= EB_INTERVALS_COUNT) idx = EB_INTERVALS_COUNT - 1;
            e->prog.next_review = today + EB_INTERVALS[idx];
        }
        e->prog.last_shown = today;
    }

    /* Render to window */
    PositionOverlayWindow(g_hWndWord);  /* ensure window fits content */
    RenderWordWindow(g_hWndWord, selected, n);

    /* Remember for stats */
    g_displayed_count = n;
    for (int i = 0; i < n; i++) {
        g_displayed[i] = selected[i];
    }
}

/* ========================================================================
 * Word import dialog (open file → parse → add to word list)
 * ======================================================================== */

static void DoImportWords(HWND hwnd) {
    wchar_t filePath[MAX_PATH] = {0};

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = hwnd;
    ofn.lpstrFilter  = L"词库文件 (*.txt)\0*.txt\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile    = filePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrTitle   = L"导入词库";
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) return;

    /* Open the imported file and parse word definitions */
    FILE* fImport = _wfopen(filePath, L"r, ccs=UTF-8");
    if (!fImport) {
        MessageBoxW(hwnd, L"无法打开导入文件。", APP_TITLE, MB_ICONERROR);
        return;
    }

    /* Open words.txt for appending */
    wchar_t wordPath[MAX_PATH];
    GetExePath(wordPath, MAX_PATH);
    wcscat(wordPath, L"\\words.txt");
    FILE* fWord = _wfopen(wordPath, L"a, ccs=UTF-8");
    if (!fWord) {
        fclose(fImport);
        MessageBoxW(hwnd, L"无法写入词库文件。", APP_TITLE, MB_ICONERROR);
        return;
    }

    int added = 0;
    wchar_t line[MAX_LINE_LEN];
    while (fgetws(line, MAX_LINE_LEN, fImport)) {
        /* strip trailing newline */
        wchar_t* p = line;
        while (*p && *p != L'\r' && *p != L'\n') p++;
        *p = L'\0';
        if (line[0] == L'\0' || line[0] == L'#') continue;

        /* extract word name (first segment before |) */
        wchar_t* sep = wcschr(line, L'|');
        if (!sep) continue;
        wchar_t saved = *sep;
        *sep = L'\0';

        /* check if already exists */
        BOOL exists = FALSE;
        for (int i = 0; i < g_word_count; i++) {
            if (wcscmp(g_words[i].def.word, line) == 0) {
                exists = TRUE;
                break;
            }
        }
        *sep = saved;

        if (exists) continue;

        /* Append to words.txt */
        fwprintf(fWord, L"%s\n", line);
        added++;

        /* Add to in-memory array */
        if (g_word_count >= g_word_capacity) {
            g_word_capacity = g_word_capacity * 3 / 2 + 10;
            if (g_word_capacity > MAX_WORDS) g_word_capacity = MAX_WORDS;
            g_words = (WordEntry*)realloc(g_words,
                          g_word_capacity * sizeof(WordEntry));
        }

        /* Parse and add */
        WordEntry* entry = &g_words[g_word_count];
        memset(entry, 0, sizeof(WordEntry));

        wchar_t* seg1 = line;
        wchar_t* seg2 = wcschr(seg1, L'|');
        if (!seg2) continue;
        *seg2 = L'\0'; seg2++;
        wchar_t* seg3 = wcschr(seg2, L'|');
        if (!seg3) continue;
        *seg3 = L'\0'; seg3++;

        wcsncpy(entry->def.word, seg1, MAX_WORD_LEN-1);
        wcsncpy(entry->def.phonetic, seg2, MAX_PHONETIC_LEN-1);
        wcsncpy(entry->def.translation, seg3, MAX_TRANS_LEN-1);
        g_word_count++;
    }

    fclose(fImport);
    fclose(fWord);

    /* Save progress with new words included */
    SaveProgressDefault();

    wchar_t msg[128];
    _snwprintf(msg, 128, L"已导入 %d 个新单词，当前共 %d 个。", added, g_word_count);
    MessageBoxW(hwnd, msg, APP_TITLE, MB_ICONINFORMATION);
}

/* ========================================================================
 * Stats dialog
 * ======================================================================== */

static void ShowStatsDialog(HWND hwndParent) {
    int seen = 0, total = g_word_count;
    int due_today = 0, new_words = 0;

    int today = TodayDays();
    for (int i = 0; i < total; i++) {
        WordProgress* p = &g_words[i].prog;
        if (p->first_seen > 0) seen++;
        if (p->first_seen == 0) new_words++;
        if (p->next_review > 0 && p->next_review <= today) due_today++;
    }

    wchar_t msg[256];
    _snwprintf(msg, 256,
               L"📊 学习统计\n\n"
               L"词库总数: %d\n"
               L"已学单词: %d (%.1f%%)\n"
               L"新词待学: %d\n"
               L"今日待复习: %d\n\n"
               L"艾宾浩斯复习周期: 1天 → 2天 → 4天 → 7天 → 15天 → 30天",
               total, seen, total > 0 ? (float)seen/total*100 : 0,
               new_words, due_today);
    MessageBoxW(hwndParent, msg, L"桌面单词 - 统计", MB_ICONINFORMATION);
}

/* ========================================================================
 * Main Window Procedure (hidden controller)
 * ======================================================================== */

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            return 0;

        case WM_TRAYICON:
            if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_LBUTTONUP) {
                ShowTrayContextMenu(hwnd, WM_RBUTTONUP);
            }
            return 0;

        case WM_TIMER:
            if (wp == ID_TIMER_REFRESH) {
                if (g_hWndWord) {
                    RefreshWordDisplay();
                }
                return 0;
            }
            if (wp == ID_TIMER_DETECT) {
                /* No longer used - window stays visible naturally via Z-order */
                return 0;
            }
            if (wp == ID_TIMER_BOTTOM) {
                return 0;
            }
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case ID_TRAY_SHOW:
                    if (g_hWndWord) {
                        if (IsWindowVisible(g_hWndWord)) {
                            FadeOutWindow(g_hWndWord);
                        } else {
                            FadeInWindow(g_hWndWord);
                            RefreshWordDisplay();
                        }
                    }
                    break;

                case ID_TRAY_SETTINGS:
                    ShowSettingsDialog(hwnd);
                    /* Refresh timer if settings changed */
                    KillTimer(hwnd, ID_TIMER_REFRESH);
                    SetTimer(hwnd, ID_TIMER_REFRESH, g_refresh_sec * 1000, NULL);
                    /* Reload background */
                    BgFreeImage();
                    if (g_bg_type == BG_IMAGE && g_bg_path[0]) {
                        BgLoadImage(g_bg_path);
                    }
                    RefreshWordDisplay();
                    break;

                case ID_TRAY_IMPORT:
                    DoImportWords(hwnd);
                    RefreshWordDisplay();
                    UpdateTrayStats();
                    break;

                case ID_TRAY_STATS:
                    ShowStatsDialog(hwnd);
                    break;

                case ID_TRAY_WORDMGR:
                    ShowWordManagerDialog(hwnd);
                    RefreshWordDisplay();
                    UpdateTrayStats();
                    break;

                case ID_TRAY_AUTOSTART: {
                    HKEY hKey;
                    BOOL on = g_auto_start;
                    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                            0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                        if (on) {
                            RegDeleteValueW(hKey, APP_NAME);
                        } else {
                            wchar_t exePath[MAX_PATH];
                            GetModuleFileNameW(NULL, exePath, MAX_PATH);
                            RegSetValueExW(hKey, APP_NAME, 0, REG_SZ,
                                          (BYTE*)exePath,
                                          (DWORD)((wcslen(exePath)+1)*sizeof(wchar_t)));
                        }
                        RegCloseKey(hKey);
                        g_auto_start = !on;
                    }
                    break;
                }

                case ID_TRAY_EXIT:
                    DestroyWindow(hwnd);
                    break;
            }
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (g_hWndWord) {
                DestroyWindow(g_hWndWord);
                g_hWndWord = NULL;
            }
            g_app_running = FALSE;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ========================================================================
 * Word overlay Window Procedure (desktop-embedded display)
 * ======================================================================== */

LRESULT CALLBACK WordWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            /* Minimal - actual rendering is done via UpdateLayeredWindow
             * in render.c. This is just to satisfy BeginPaint/EndPaint. */
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;  /* we render the background ourselves via layered window */

        case WM_DISPLAYCHANGE:
            /* Screen res changed — reposition and re-render */
            if (g_hWndWord) {
                PositionOverlayWindow(g_hWndWord);
                RefreshWordDisplay();
            }
            return 0;

        case WM_LBUTTONDBLCLK: {
            /* User double-clicked a word → mark as familiar */
            int my = GET_Y_LPARAM(lp);

            /* Find which word was clicked */
            for (int i = 0; i < g_displayed_count; i++) {
                if (my >= g_word_pos_y[i] &&
                    my <= g_word_pos_y[i] + g_word_pos_h[i]) {
                    WordEntry* entry = g_displayed[i];
                    if (entry && !entry->prog.familiar) {
                        entry->prog.familiar = TRUE;
                        {
                            wchar_t famPath[MAX_PATH];
                            GetFamiliarPath(famPath, MAX_PATH);
                            AppendFamiliarWord(famPath, entry->def.word);
                        }

                        /* Remove this word from displayed array */
                        for (int j = i; j < g_displayed_count - 1; j++) {
                            g_displayed[j] = g_displayed[j + 1];
                        }
                        g_displayed_count--;
                        g_hovered_index = -1;

                        /* Re-render remaining words, or pick new if none left */
                        if (g_displayed_count > 0) {
                            RenderWordWindow(hwnd, g_displayed, g_displayed_count);
                        } else {
                            RefreshWordDisplay();
                        }
                    }
                    break;
                }
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            /* Right-click: show context menu with "标记熟悉" option */
            int my = GET_Y_LPARAM(lp);

            for (int i = 0; i < g_displayed_count; i++) {
                if (my >= g_word_pos_y[i] &&
                    my <= g_word_pos_y[i] + g_word_pos_h[i]) {
                    WordEntry* entry = g_displayed[i];
                    if (entry && !entry->prog.familiar) {
                        HMENU hMenu = CreatePopupMenu();
                        AppendMenuW(hMenu, MF_STRING, 1, L"标记为熟悉（不再出现）");
                        POINT pt;
                        GetCursorPos(&pt);
                        SetForegroundWindow(hwnd);
                        int cmd = TrackPopupMenu(hMenu,
                            TPM_RETURNCMD | TPM_RIGHTBUTTON,
                            pt.x, pt.y, 0, hwnd, NULL);
                        if (cmd == 1) {
                            entry->prog.familiar = TRUE;
                            {
                                wchar_t famPath[MAX_PATH];
                                GetFamiliarPath(famPath, MAX_PATH);
                                AppendFamiliarWord(famPath, entry->def.word);
                            }

                            /* Remove this word from displayed array */
                            int found_i = i;
                            for (int j = found_i; j < g_displayed_count - 1; j++) {
                                g_displayed[j] = g_displayed[j + 1];
                            }
                            g_displayed_count--;
                            g_hovered_index = -1;

                            /* Re-render remaining words, or pick new if none left */
                            if (g_displayed_count > 0) {
                                RenderWordWindow(hwnd, g_displayed, g_displayed_count);
                            } else {
                                RefreshWordDisplay();
                            }
                        }
                        DestroyMenu(hMenu);
                    }
                    break;
                }
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            int my = GET_Y_LPARAM(lp);
            int new_hover = -1;

            /* Find which word the mouse is over */
            for (int i = 0; i < g_displayed_count; i++) {
                if (my >= g_word_pos_y[i] &&
                    my <= g_word_pos_y[i] + g_word_pos_h[i]) {
                    new_hover = i;
                    break;
                }
            }

            if (new_hover != g_hovered_index) {
                g_hovered_index = new_hover;

                if (new_hover >= 0) {
                    /* Mouse entered a word - pause refresh timer (if enabled) */
                    if (g_hover_pause && !g_timer_paused) {
                        KillTimer(g_hWndMain, ID_TIMER_REFRESH);
                        g_timer_paused = TRUE;
                    }
                    /* Request WM_MOUSELEAVE notification */
                    TRACKMOUSEEVENT tme;
                    tme.cbSize = sizeof(TRACKMOUSEEVENT);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = hwnd;
                    TrackMouseEvent(&tme);
                } else {
                    /* Mouse left word (but still in window) - resume timer (if was paused) */
                    if (g_hover_pause && g_timer_paused) {
                        SetTimer(g_hWndMain, ID_TIMER_REFRESH,
                                 g_refresh_sec * 1000, NULL);
                        g_timer_paused = FALSE;
                    }
                }

                /* Re-render with updated hover state */
                if (g_displayed_count > 0) {
                    RenderWordWindow(hwnd, g_displayed, g_displayed_count);
                }
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            /* Mouse completely left the window */
            g_hovered_index = -1;
            if (g_hover_pause && g_timer_paused) {
                SetTimer(g_hWndMain, ID_TIMER_REFRESH,
                         g_refresh_sec * 1000, NULL);
                g_timer_paused = FALSE;
            }
            /* Re-render without hover effect */
            if (g_displayed_count > 0) {
                RenderWordWindow(hwnd, g_displayed, g_displayed_count);
            }
            return 0;
        }

        case WM_CLOSE:
            return 0;  /* don't close via Alt+F4 */
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
