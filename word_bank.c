#include "main.h"
#include "darkmode.h"

/* Word manager dialog — single instance */
static HWND g_hWndBank = NULL;

/* Child controls */
static HWND g_hLV       = NULL;
static HWND g_hBtnMark  = NULL;
static HWND g_hBtnClear = NULL;
static HWND g_hLblStats = NULL;

/* Control IDs */
#define ID_FILTER_COMBO  201
#define ID_BTN_MARK      202
#define ID_BTN_CLEAR     203
#define ID_LV_WORDS      210
#define ID_LBL_STATS     211

/* Current filter: 0=all, 1=learning, 2=familiar */
static int g_filter_mode = 0;

/* Is the list being populated? (suppress LVN_ITEMCHANGED during fill) */
static BOOL g_populating = FALSE;

/* --- Helpers --- */

/* Does a word pass the current filter? */
static BOOL PassesFilter(WordEntry* e, int mode) {
    switch (mode) {
        case 0: return TRUE;                        /* all */
        case 1: return !e->prog.familiar;           /* learning (new + in-progress) */
        case 2: return e->prog.familiar;            /* familiar */
        default: return TRUE;
    }
}

/* Get status display string */
static const wchar_t* StatusText(WordEntry* e) {
    if (e->prog.familiar)    return L"熟悉";
    if (e->prog.first_seen > 0) return L"学习中";
    return L"新词";
}

/* Rebuild the ListView with current filter */
static void RefreshListView(void) {
    if (!g_hLV) return;

    g_populating = TRUE;

    /* Delete all items */
    ListView_DeleteAllItems(g_hLV);

    int visible = 0, familiar_count = 0;

    for (int i = 0; i < g_word_count; i++) {
        if (!PassesFilter(&g_words[i], g_filter_mode)) continue;

        LVITEMW lvi = {0};
        lvi.mask     = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem    = visible;
        lvi.pszText  = g_words[i].def.word;
        lvi.lParam   = (LPARAM)i;  /* store WordEntry index */
        ListView_InsertItem(g_hLV, &lvi);

        /* Column 1: phonetic */
        { LVITEMW lv = {0}; lv.iSubItem = 1; lv.pszText = g_words[i].def.phonetic;
          SendMessageW(g_hLV, LVM_SETITEMTEXTW, (WPARAM)visible, (LPARAM)&lv); }

        /* Column 2: translation */
        { LVITEMW lv = {0}; lv.iSubItem = 2; lv.pszText = g_words[i].def.translation;
          SendMessageW(g_hLV, LVM_SETITEMTEXTW, (WPARAM)visible, (LPARAM)&lv); }

        /* Column 3: status */
        { LVITEMW lv = {0}; lv.iSubItem = 3; lv.pszText = (LPWSTR)StatusText(&g_words[i]);
          SendMessageW(g_hLV, LVM_SETITEMTEXTW, (WPARAM)visible, (LPARAM)&lv); }

        visible++;
    }

    /* Count familiar words (for stats — always from full list) */
    for (int i = 0; i < g_word_count; i++) {
        if (g_words[i].prog.familiar) familiar_count++;
    }

    g_populating = FALSE;

    /* Disable buttons initially, will be updated on selection change */
    EnableWindow(g_hBtnMark,  FALSE);
    EnableWindow(g_hBtnClear, FALSE);

    /* Update stats label */
    wchar_t buf[128];
    _snwprintf(buf, 128, L"显示: %d  熟悉: %d  总计: %d",
               visible, familiar_count, g_word_count);
    SetWindowTextW(g_hLblStats, buf);
}

/* Toggle familiar flag for selection: set_familiar = TRUE to mark, FALSE to clear */
static void ToggleFamiliar(BOOL set_familiar) {
    if (!g_hLV) return;

    int count = ListView_GetSelectedCount(g_hLV);
    if (count == 0) return;

    /* Determine actual count of selected items */
    int sel_count = ListView_GetItemCount(g_hLV);
    int changed = 0;

    for (int i = 0; i < sel_count; i++) {
        if (ListView_GetItemState(g_hLV, i, LVIS_SELECTED) & LVIS_SELECTED) {
            LVITEMW lvi = {0};
            lvi.iItem = i;
            lvi.mask  = LVIF_PARAM;
            ListView_GetItem(g_hLV, &lvi);
            int idx = (int)lvi.lParam;
            if (idx >= 0 && idx < g_word_count) {
                if (g_words[idx].prog.familiar != set_familiar) {
                    g_words[idx].prog.familiar = set_familiar;
                    changed++;
                }
            }
        }
    }

    if (changed > 0) {
        /* Rewrite familiar.dat with all currently-familiar words */
        {
            wchar_t famPath[MAX_PATH];
            GetFamiliarPath(famPath, MAX_PATH);
            SaveFamiliarWords(famPath);
        }

        /* Refresh list — items may move between filter views */
        RefreshListView();

        /* If marking words as familiar, remove them from displayed set */
        if (set_familiar && g_hWndWord) {
            BOOL affected = FALSE;
            for (int i = 0; i < g_displayed_count; i++) {
                if (g_displayed[i] && g_displayed[i]->prog.familiar) {
                    /* Remove this word from displayed array */
                    for (int j = i; j < g_displayed_count - 1; j++) {
                        g_displayed[j] = g_displayed[j + 1];
                    }
                    g_displayed_count--;
                    i--;  /* recheck this index */
                    affected = TRUE;
                }
            }
            if (affected) {
                g_hovered_index = -1;
                if (g_displayed_count > 0) {
                    RenderWordWindow(g_hWndWord, g_displayed, g_displayed_count);
                } else {
                    /* Trigger immediate refresh via timer message */
                    PostMessageW(g_hWndMain, WM_TIMER, ID_TIMER_REFRESH, 0);
                }
            }
        }
    }
}

/* --- Window Procedure --- */

static LRESULT CALLBACK WordBankWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hi = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

            /* Apply dark title bar */
            if (DarkMode_IsEnabled()) {
                DarkMode_ApplyTitleBar(hwnd);
            }

            /* Filter label + combo */
            CreateWindowW(L"STATIC", L"筛选:", WS_CHILD | WS_VISIBLE,
                          12, 18, 36, 20, hwnd, NULL, hi, NULL);
            HWND hCmb = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE |
                          CBS_DROPDOWNLIST, 52, 14, 85, 200,
                          hwnd, (HMENU)ID_FILTER_COMBO, hi, NULL);
            SendMessageW(hCmb, CB_ADDSTRING, 0, (LPARAM)L"全部");
            SendMessageW(hCmb, CB_ADDSTRING, 0, (LPARAM)L"学习中");
            SendMessageW(hCmb, CB_ADDSTRING, 0, (LPARAM)L"熟悉");
            SendMessageW(hCmb, CB_SETCURSEL, 0, 0);

            /* Mark / Clear familiar buttons */
            g_hBtnMark = CreateWindowW(L"BUTTON", L"标记为熟悉",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                160, 12, 95, 26, hwnd, (HMENU)ID_BTN_MARK, hi, NULL);
            EnableWindow(g_hBtnMark, FALSE);

            g_hBtnClear = CreateWindowW(L"BUTTON", L"取消熟悉",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                265, 12, 95, 26, hwnd, (HMENU)ID_BTN_CLEAR, hi, NULL);
            EnableWindow(g_hBtnClear, FALSE);

            /* ListView — positioned below toolbar */
            g_hLV = CreateWindowW(WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER |
                LVS_REPORT | LVS_SHOWSELALWAYS,
                10, 52, 0, 0,  /* size set in WM_SIZE */
                hwnd, (HMENU)ID_LV_WORDS, hi, NULL);

            /* Enable full-row select + extended selection */
            ListView_SetExtendedListViewStyle(g_hLV,
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            /* Apply dark mode to child controls */
            DarkMode_ApplyTheme(hwnd);

            /* Columns */
            LVCOLUMNW lvc = {0};
            lvc.mask = LVCF_TEXT | LVCF_WIDTH;
            lvc.pszText = L"单词"; lvc.cx = 120;
            ListView_InsertColumn(g_hLV, 0, &lvc);
            lvc.pszText = L"音标"; lvc.cx = 140;
            ListView_InsertColumn(g_hLV, 1, &lvc);
            lvc.pszText = L"释义"; lvc.cx = 230;
            ListView_InsertColumn(g_hLV, 2, &lvc);
            lvc.pszText = L"状态"; lvc.cx = 80;
            ListView_InsertColumn(g_hLV, 3, &lvc);

            /* Stats label */
            g_hLblStats = CreateWindowW(L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 0, 0, 0, hwnd, (HMENU)ID_LBL_STATS, hi, NULL);

            /* Close button */
            CreateWindowW(L"BUTTON", L"关闭", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 70, 26, hwnd, (HMENU)IDCANCEL, hi, NULL);

            /* Load data */
            RefreshListView();
            return 0;
        }

        case WM_SIZE: {
            int cw = LOWORD(lp);  /* client width */
            int ch = HIWORD(lp);  /* client height */

            /* Resize ListView */
            if (g_hLV) {
                SetWindowPos(g_hLV, NULL, 10, 52,
                    cw - 24, ch - 100, SWP_NOZORDER);
            }

            /* Reposition stats label */
            if (g_hLblStats) {
                SetWindowPos(g_hLblStats, NULL, 10, ch - 38,
                    300, 20, SWP_NOZORDER);
            }

            /* Reposition Close button */
            HWND hClose = GetDlgItem(hwnd, IDCANCEL);
            if (hClose) {
                SetWindowPos(hClose, NULL, cw - 90, ch - 40,
                    70, 26, SWP_NOZORDER);
            }
            return 0;
        }

        case WM_COMMAND:
            if (HIWORD(wp) == CBN_SELCHANGE && LOWORD(wp) == ID_FILTER_COMBO) {
                g_filter_mode = (int)SendMessageW(GetDlgItem(hwnd, ID_FILTER_COMBO),
                                                  CB_GETCURSEL, 0, 0);
                RefreshListView();
                return 0;
            }
            if (LOWORD(wp) == ID_BTN_MARK) {
                ToggleFamiliar(TRUE);
                return 0;
            }
            if (LOWORD(wp) == ID_BTN_CLEAR) {
                ToggleFamiliar(FALSE);
                return 0;
            }
            if (LOWORD(wp) == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_NOTIFY: {
            NMHDR* nmh = (NMHDR*)lp;
            if (nmh->idFrom == ID_LV_WORDS && nmh->code == LVN_ITEMCHANGED) {
                if (g_populating) return 0;

                /* Update button states based on selection */
                BOOL has_familiar = FALSE, has_nonfamiliar = FALSE;

                int count = ListView_GetItemCount(g_hLV);
                for (int i = 0; i < count; i++) {
                    if (ListView_GetItemState(g_hLV, i, LVIS_SELECTED) & LVIS_SELECTED) {
                        LVITEMW lvi = {0};
                        lvi.iItem = i;
                        lvi.mask  = LVIF_PARAM;
                        ListView_GetItem(g_hLV, &lvi);
                        int idx = (int)lvi.lParam;
                        if (idx >= 0 && idx < g_word_count) {
                            if (g_words[idx].prog.familiar)
                                has_familiar = TRUE;
                            else
                                has_nonfamiliar = TRUE;
                        }
                    }
                }

                EnableWindow(g_hBtnMark,  has_nonfamiliar);
                EnableWindow(g_hBtnClear, has_familiar);
                return 0;
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            COLORREF textClr = 0;
            if (DarkMode_OnCtlStatic((HDC)wp, &textClr)) {
                return (LRESULT)DarkMode_GetBgBrush();
            }
            break;
        }
        case WM_CTLCOLORBTN: {
            if (DarkMode_OnCtlButton((HDC)wp)) {
                return (LRESULT)DarkMode_GetBgBrush();
            }
            break;
        }
        case WM_CTLCOLORLISTBOX: {
            if (DarkMode_IsEnabled()) {
                /* ComboBox dropdown list in dark mode */
                SetBkColor((HDC)wp, DM_BG);
                SetTextColor((HDC)wp, DM_TEXT);
                return (LRESULT)DarkMode_GetBgBrush();
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            g_hLV = NULL;
            g_hBtnMark = NULL;
            g_hBtnClear = NULL;
            g_hLblStats = NULL;
            g_hWndBank = NULL;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* --- Public entry point --- */

void ShowWordManagerDialog(HWND hwndParent) {
    if (g_hWndBank) {
        SetForegroundWindow(g_hWndBank);
        return;
    }

    /* Register class (one-time) */
    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = WordBankWndProc;
    wc.hInstance     = g_hInst;
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = DarkMode_IsEnabled() ? DarkMode_GetBgBrush()
                                             : (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"DesktopWordBank";
    static BOOL registered = FALSE;
    if (!registered) {
        RegisterClassW(&wc);
        registered = TRUE;
    }

    g_filter_mode = 0;  /* reset to "all" on open */

    g_hWndBank = CreateWindowW(L"DesktopWordBank", L"桌面单词 - 词库管理",
        WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX),
        CW_USEDEFAULT, CW_USEDEFAULT,
        620, 440, hwndParent, NULL, g_hInst, NULL);
    ShowWindow(g_hWndBank, SW_SHOW);
    SetForegroundWindow(g_hWndBank);
}
