#include "main.h"

/* Keep our overlay window at the bottom of the Z-order.
 * Unlike SetParent(WorkerW), this approach works reliably on Win10/11. */
void KeepWindowAtBottom(HWND hwnd) {
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

/* Position window at the top-right of the screen.
 * Height is calculated from display_count and font sizes so all
 * words are visible. */
void PositionOverlayWindow(HWND hwnd) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int wndW = screenW / 4;
    int wndX = screenW - wndW;

    /* Height: padding + N words × (word_font + sub_font space).
       Use non-hover (base) font sizes for calculation. */
    int padding   = 20;
    int per_word  = g_font_large + 12 + g_font_small * 2 + 10;
    int wndH      = padding + g_display_count * per_word;
    if (wndH < 100)  wndH = 100;
    if (wndH > screenH) wndH = screenH;

    SetWindowPos(hwnd, NULL, wndX, 0, wndW, wndH,
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

/* Use EnumWindows to properly check all top-level windows */
typedef struct {
    BOOL found_visible;
} EnumData;

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    EnumData* data = (EnumData*)lParam;
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (IsIconic(hwnd)) return TRUE;

    wchar_t cls[64];
    GetClassNameW(hwnd, cls, 64);

    /* Skip desktop-related and our own windows */
    if (_wcsicmp(cls, L"Progman") == 0) return TRUE;
    if (_wcsicmp(cls, L"WorkerW") == 0) return TRUE;
    if (_wcsicmp(cls, L"Shell_TrayWnd") == 0) return TRUE;
    if (_wcsicmp(cls, L"DesktopWordOverlay") == 0) return TRUE;
    if (_wcsicmp(cls, L"DesktopWordMain") == 0) return TRUE;
    if (_wcsicmp(cls, L"Windows.UI.Core.CoreWindow") == 0) return TRUE;

    data->found_visible = TRUE;
    return FALSE; /* stop enumeration */
}

/* Check if the desktop is currently visible.
 * Returns TRUE if user is on the desktop. */
BOOL IsDesktopVisible(void) {
    HWND fg = GetForegroundWindow();
    if (!fg) return TRUE;

    wchar_t cls[64];
    GetClassNameW(fg, cls, 64);

    /* Common desktop classes */
    if (_wcsicmp(cls, L"Progman") == 0) return TRUE;
    if (_wcsicmp(cls, L"WorkerW") == 0) return TRUE;
    if (_wcsicmp(cls, L"Shell_TrayWnd") == 0) return TRUE;
    if (_wcsicmp(cls, L"Windows.UI.Core.CoreWindow") == 0) return TRUE;

    /* Check if desktop window itself has focus */
    HWND shell = GetShellWindow();
    if (fg == shell) return TRUE;

    /* Use EnumWindows to check if any visible non-desktop window exists */
    EnumData data = { FALSE };
    EnumWindows(EnumWindowsProc, (LPARAM)&data);

    return !data.found_visible;
}
