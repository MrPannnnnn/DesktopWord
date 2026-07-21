#include "darkmode.h"
#include <uxtheme.h>

/* DWMWA_USE_IMMERSIVE_DARK_MODE may not be in older MinGW headers */
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

/* --- Dark mode detection --- */

BOOL DarkMode_IsEnabled(void) {
    HKEY hKey;
    DWORD value = 1;  /* default: light */
    DWORD size = sizeof(value);

    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, NULL,
                        (BYTE*)&value, &size);
        RegCloseKey(hKey);
    }
    return (value == 0);  /* 0 = dark, 1 = light */
}

/* --- Dark title bar for top-level windows --- */

void DarkMode_ApplyTitleBar(HWND hwnd) {
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (!hDwm) return;

    typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(
        HWND, DWORD, LPCVOID, DWORD);
    PFN_DwmSetWindowAttribute pfn =
        (PFN_DwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");

    if (pfn) {
        BOOL dark = TRUE;
        pfn(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    }

    FreeLibrary(hDwm);
}

/* --- Dark theme for child controls --- */

void DarkMode_ApplyTheme(HWND hwnd) {
    if (!DarkMode_IsEnabled()) return;

    /* Walk child windows and apply dark themes */
    HWND hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild) {
        wchar_t cls[32];
        GetClassNameW(hChild, cls, 32);

        if (wcscmp(cls, L"SysListView32") == 0) {
            /* Explorer-style dark list view */
            SetWindowTheme(hChild, L"DarkMode_Explorer", NULL);
            /* Dark background for empty areas */
            ListView_SetBkColor(hChild, DM_BG);
            ListView_SetTextBkColor(hChild, DM_BG);
            ListView_SetTextColor(hChild, DM_TEXT);
        }
        else if (wcscmp(cls, L"ComboBox") == 0) {
            SetWindowTheme(hChild, L"DarkMode_CFD", NULL);
        }
        else if (wcscmp(cls, WC_LISTBOXW) == 0 ||
                 wcscmp(cls, L"ComboLBox") == 0) {
            SetWindowTheme(hChild, L"DarkMode_Explorer", NULL);
        }
        else if (wcscmp(cls, WC_BUTTONW) == 0) {
            /* Let WM_CTLCOLORBTN handle button background */
        }

        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }
}

/* --- Brushes and colors --- */

static HBRUSH g_hBrBg = NULL;

HBRUSH DarkMode_GetBgBrush(void) {
    if (!g_hBrBg) {
        g_hBrBg = CreateSolidBrush(DM_BG);
    }
    return g_hBrBg;
}

COLORREF DarkMode_GetTextColor(void) {
    return DM_TEXT;
}

COLORREF DarkMode_GetBgColor(void) {
    return DM_BG;
}

/* --- WM_CTLCOLORSTATIC handling --- */

BOOL DarkMode_OnCtlStatic(HDC hdc, COLORREF* textColor) {
    if (!DarkMode_IsEnabled()) return FALSE;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, DM_TEXT);
    *textColor = DM_TEXT;
    return TRUE;
}

/* --- WM_CTLCOLORBTN handling --- */

BOOL DarkMode_OnCtlButton(HDC hdc) {
    if (!DarkMode_IsEnabled()) return FALSE;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, DM_TEXT);
    return TRUE;
}
