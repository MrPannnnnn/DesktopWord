#include "main.h"
#include "darkmode.h"

/* Default settings */
int     g_refresh_sec   = 30;
int     g_display_count = 2;
int     g_font_large    = 36;
int     g_font_small    = 16;
BOOL    g_auto_start    = FALSE;
BOOL    g_hover_pause   = TRUE;
int     g_bg_type       = BG_TRANSPARENT;
wchar_t g_bg_path[MAX_PATH] = {0};

/* settings dialog handles */
static HWND g_hDlg = NULL;
static HWND g_hEdtRefresh = NULL, g_hEdtCount = NULL;
static HWND g_hEdtFontLarge = NULL, g_hEdtFontSmall = NULL;

void LoadSettings(void) {
    wchar_t iniPath[MAX_PATH];
    GetAppDataPath(iniPath, MAX_PATH);
    wcscat(iniPath, L"\\settings.ini");

    g_refresh_sec   = GetPrivateProfileIntW(L"General", L"RefreshSec", 30, iniPath);
    g_display_count = GetPrivateProfileIntW(L"General", L"DisplayCount", 2, iniPath);
    g_font_large    = GetPrivateProfileIntW(L"General", L"FontLarge", 36, iniPath);
    g_font_small    = GetPrivateProfileIntW(L"General", L"FontSmall", 16, iniPath);
    g_hover_pause   = GetPrivateProfileIntW(L"General", L"HoverPause", 1, iniPath);

    if (g_refresh_sec < 5)   g_refresh_sec = 5;
    if (g_refresh_sec > 600) g_refresh_sec = 600;
    if (g_display_count < 1)  g_display_count = 1;
    if (g_display_count > MAX_DISPLAY_COUNT) g_display_count = MAX_DISPLAY_COUNT;
    if (g_font_large < 16)    g_font_large = 16;
    if (g_font_large > 72)    g_font_large = 72;
    if (g_font_small < 10)    g_font_small = 10;
    if (g_font_small > 36)    g_font_small = 36;

    g_bg_type = GetPrivateProfileIntW(L"Background", L"Type", BG_TRANSPARENT, iniPath);
    GetPrivateProfileStringW(L"Background", L"Path", L"", g_bg_path, MAX_PATH, iniPath);

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t val[8] = {0};
        DWORD size = sizeof(val);
        g_auto_start = (RegQueryValueExW(hKey, APP_NAME, NULL, NULL,
                          (BYTE*)val, &size) == ERROR_SUCCESS && val[0]);
        RegCloseKey(hKey);
    }
}

void SaveSettings(void) {
    wchar_t iniPath[MAX_PATH];
    GetAppDataPath(iniPath, MAX_PATH);
    wcscat(iniPath, L"\\settings.ini");

    wchar_t buf[32];
    _snwprintf(buf, 32, L"%d", g_refresh_sec);
    WritePrivateProfileStringW(L"General", L"RefreshSec", buf, iniPath);
    _snwprintf(buf, 32, L"%d", g_display_count);
    WritePrivateProfileStringW(L"General", L"DisplayCount", buf, iniPath);
    _snwprintf(buf, 32, L"%d", g_font_large);
    WritePrivateProfileStringW(L"General", L"FontLarge", buf, iniPath);
    _snwprintf(buf, 32, L"%d", g_font_small);
    WritePrivateProfileStringW(L"General", L"FontSmall", buf, iniPath);
    _snwprintf(buf, 32, L"%d", g_hover_pause);
    WritePrivateProfileStringW(L"General", L"HoverPause", buf, iniPath);

    _snwprintf(buf, 32, L"%d", g_bg_type);
    WritePrivateProfileStringW(L"Background", L"Type", buf, iniPath);
    WritePrivateProfileStringW(L"Background", L"Path", g_bg_path, iniPath);
}

/* Programmatic settings dialog */
static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hi = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

            /* Apply dark mode if system is in dark theme */
            if (DarkMode_IsEnabled()) {
                DarkMode_ApplyTitleBar(hwnd);
            }

            /* Layout: left=15, label_w=115, input_x=145, input_w=90, row_h=38, gap=8 */
            #define LX  15
            #define LW  115
            #define IX  145
            #define IW  90
            #define RH  38
            #define GAP 8

            int y = 12;

            /* --- Word settings --- */
            CreateWindowW(L"BUTTON", L"单词设置", WS_CHILD|WS_VISIBLE|BS_GROUPBOX,
                          10, y, 260, 165, hwnd, NULL, hi, NULL);
            y += 18;

            CreateWindowW(L"STATIC", L"刷新间隔(秒):", WS_CHILD|WS_VISIBLE,
                          LX, y+4, LW, 18, hwnd, NULL, hi, NULL);
            g_hEdtRefresh = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_NUMBER,
                           IX, y, IW, 22, hwnd, (HMENU)101, hi, NULL);

            y += RH;
            CreateWindowW(L"STATIC", L"每次单词数(1-20):", WS_CHILD|WS_VISIBLE,
                          LX, y+4, LW, 18, hwnd, NULL, hi, NULL);
            g_hEdtCount = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_NUMBER,
                           IX, y, IW, 22, hwnd, (HMENU)102, hi, NULL);

            y += RH;
            CreateWindowW(L"STATIC", L"大字字号:", WS_CHILD|WS_VISIBLE,
                          LX, y+4, LW, 18, hwnd, NULL, hi, NULL);
            g_hEdtFontLarge = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_NUMBER,
                              IX, y, IW, 22, hwnd, (HMENU)103, hi, NULL);

            y += RH;
            CreateWindowW(L"STATIC", L"小字字号:", WS_CHILD|WS_VISIBLE,
                          LX, y+4, LW, 18, hwnd, NULL, hi, NULL);
            g_hEdtFontSmall = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_NUMBER,
                              IX, y, IW, 22, hwnd, (HMENU)104, hi, NULL);
            y += RH - 8;
            /* --- end Word settings group --- */
            y += 12;

            /* --- Behavior settings --- */
            CreateWindowW(L"BUTTON", L"行为设置", WS_CHILD|WS_VISIBLE|BS_GROUPBOX,
                          10, y, 260, 200, hwnd, NULL, hi, NULL);
            y += 18;

            {
                HWND hChk = CreateWindowW(L"BUTTON", L"鼠标悬停时暂停刷新",
                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                    LX, y, 200, 22, hwnd, (HMENU)108, hi, NULL);
                SendMessageW(hChk, BM_SETCHECK,
                    g_hover_pause ? BST_CHECKED : BST_UNCHECKED, 0);
            }

            y += RH;
            CreateWindowW(L"STATIC", L"背景模式:", WS_CHILD|WS_VISIBLE,
                          LX, y+4, LW, 18, hwnd, NULL, hi, NULL);
            HWND hCmb = CreateWindowW(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
                          IX, y, IW, 100, hwnd, (HMENU)105, hi, NULL);
            SendMessageW(hCmb, CB_ADDSTRING, 0, (LPARAM)L"透明");
            SendMessageW(hCmb, CB_ADDSTRING, 0, (LPARAM)L"图片");
            SendMessageW(hCmb, CB_SETCURSEL, g_bg_type, 0);

            y += RH - 8;
            HWND hBtn = CreateWindowW(L"BUTTON", L"选择背景...", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
                          IX, y, 90, 24, hwnd, (HMENU)106, hi, NULL);
            if (g_bg_type != BG_IMAGE) EnableWindow(hBtn, FALSE);

            y += RH - 6;
            CreateWindowW(L"STATIC", g_bg_path[0] ? g_bg_path : L"(未选择)",
                          WS_CHILD|WS_VISIBLE|SS_LEFT,
                          LX, y, 240, 16, hwnd, (HMENU)107, hi, NULL);
            y += 24;
            /* --- end Behavior settings group --- */
            y += 10;

            /* --- Action buttons --- */
            int btn_w = 80, btn_h = 28;
            CreateWindowW(L"BUTTON", L"确定", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
                          140 - btn_w - 10, y, btn_w, btn_h, hwnd, (HMENU)IDOK, hi, NULL);
            CreateWindowW(L"BUTTON", L"取消", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
                          140 + 10, y, btn_w, btn_h, hwnd, (HMENU)IDCANCEL, hi, NULL);

            /* Set current values */
            wchar_t buf[16];
            _snwprintf(buf, 16, L"%d", g_refresh_sec);
            SetWindowTextW(g_hEdtRefresh, buf);
            _snwprintf(buf, 16, L"%d", g_display_count);
            SetWindowTextW(g_hEdtCount, buf);
            _snwprintf(buf, 16, L"%d", g_font_large);
            SetWindowTextW(g_hEdtFontLarge, buf);
            _snwprintf(buf, 16, L"%d", g_font_small);
            SetWindowTextW(g_hEdtFontSmall, buf);
            return 0;
        }
        case WM_COMMAND:
            if (HIWORD(wp) == CBN_SELCHANGE && LOWORD(wp) == 105) {
                /* Background type changed */
                HWND hCmb = GetDlgItem(hwnd, 105);
                int sel = (int)SendMessageW(hCmb, CB_GETCURSEL, 0, 0);
                HWND hBtn = GetDlgItem(hwnd, 106);
                EnableWindow(hBtn, (sel == BG_IMAGE));
                return 0;
            }
            if (LOWORD(wp) == 106) {
                /* Choose background image */
                wchar_t filePath[MAX_PATH] = {0};
                OPENFILENAMEW ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = L"图片文件 (*.png;*.jpg;*.jpeg;*.bmp;*.gif)\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0所有文件 (*.*)\0*.*\0";
                ofn.lpstrFile = filePath;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrTitle = L"选择背景图片";
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) {
                    wcscpy(g_bg_path, filePath);
                    SetWindowTextW(GetDlgItem(hwnd, 107), filePath);
                }
                return 0;
            }
            if (LOWORD(wp) == IDOK) {
                wchar_t buf[16];
                GetWindowTextW(g_hEdtRefresh, buf, 16);
                g_refresh_sec = _wtoi(buf);
                GetWindowTextW(g_hEdtCount, buf, 16);
                g_display_count = _wtoi(buf);
                GetWindowTextW(g_hEdtFontLarge, buf, 16);
                g_font_large = _wtoi(buf);
                GetWindowTextW(g_hEdtFontSmall, buf, 16);
                g_font_small = _wtoi(buf);

                if (g_refresh_sec < 5) g_refresh_sec = 5;
                if (g_display_count < 1) g_display_count = 1;
                if (g_display_count > MAX_DISPLAY_COUNT) g_display_count = MAX_DISPLAY_COUNT;
                if (g_font_large < 16) g_font_large = 16;
                if (g_font_small < 10) g_font_small = 10;

                /* Read hover-pause checkbox */
                {
                    HWND hChk = GetDlgItem(hwnd, 108);
                    g_hover_pause = (SendMessageW(hChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                }

                /* Save bg type */
                HWND hCmb = GetDlgItem(hwnd, 105);
                g_bg_type = (int)SendMessageW(hCmb, CB_GETCURSEL, 0, 0);

                SaveSettings();
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wp) == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
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
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_hDlg = NULL;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ShowSettingsDialog(HWND hwndParent) {
    if (g_hDlg) {
        SetForegroundWindow(g_hDlg);
        return;
    }

    /* Register settings window class (one-time) */
    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = SettingsWndProc;
    wc.hInstance     = g_hInst;
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = DarkMode_IsEnabled() ? DarkMode_GetBgBrush()
                                             : (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"DesktopWordSettings";
    static BOOL registered = FALSE;
    if (!registered) {
        RegisterClassW(&wc);
        registered = TRUE;
    }

    g_hDlg = CreateWindowW(L"DesktopWordSettings", L"桌面单词 - 设置",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           290, 480, hwndParent, NULL, g_hInst, NULL);
    ShowWindow(g_hDlg, SW_SHOW);
    SetForegroundWindow(g_hDlg);
}
