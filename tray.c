#include "main.h"

static NOTIFYICONDATAW g_nid = {0};
static HMENU g_hTrayMenu = NULL;

BOOL CreateTrayIcon(HWND hwnd) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd             = hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wcscpy(g_nid.szTip, L"桌面单词");

    return Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void RemoveTrayIcon(void) {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

void ShowTrayContextMenu(HWND hwnd, LPARAM lParam) {
    if (lParam != WM_RBUTTONUP) return;

    if (g_hTrayMenu) DestroyMenu(g_hTrayMenu);
    g_hTrayMenu = CreatePopupMenu();

    wchar_t autoText[32];
    wcscpy(autoText, g_auto_start ? L"✓ 开机自启" : L"  开机自启");

    AppendMenuW(g_hTrayMenu, MF_STRING, ID_TRAY_SHOW, L"显示/隐藏单词");
    AppendMenuW(g_hTrayMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(g_hTrayMenu, MF_STRING, ID_TRAY_SETTINGS, L"设置...");
    AppendMenuW(g_hTrayMenu, MF_STRING, ID_TRAY_IMPORT, L"导入词库...");
    AppendMenuW(g_hTrayMenu, MF_STRING, ID_TRAY_STATS, L"学习统计");
    AppendMenuW(g_hTrayMenu, MF_STRING, ID_TRAY_WORDMGR, L"词库管理...");
    AppendMenuW(g_hTrayMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(g_hTrayMenu, MF_STRING, ID_TRAY_AUTOSTART, autoText);
    AppendMenuW(g_hTrayMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(g_hTrayMenu, MF_STRING, ID_TRAY_EXIT, L"退出");

    /* Show at cursor position */
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);  /* needed for TrackPopupMenu to work */
    TrackPopupMenu(g_hTrayMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                   pt.x, pt.y, 0, hwnd, NULL);
    PostMessageW(hwnd, WM_NULL, 0, 0);  /* benign message to fix menu */
}

/* Update tray tip with stats */
void UpdateTrayStats(void) {
    if (!g_nid.hWnd) return;

    int seen_count = 0;
    for (int i = 0; i < g_word_count; i++) {
        if (g_words[i].prog.first_seen > 0) seen_count++;
    }

    _snwprintf(g_nid.szTip, 128, L"桌面单词 - 已学 %d/%d", seen_count, g_word_count);
    g_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
