#ifndef DARKMODE_H
#define DARKMODE_H

#include <windows.h>

/* Check Windows dark mode setting via registry */
BOOL DarkMode_IsEnabled(void);

/* Apply dark title bar to a window (Windows 10 1809+ / 11) */
void DarkMode_ApplyTitleBar(HWND hwnd);

/* Apply dark theme to child controls (ListView, ComboBox, etc.) */
void DarkMode_ApplyTheme(HWND hwnd);

/* Dark background brush for dialogs */
HBRUSH DarkMode_GetBgBrush(void);

/* Dark text color */
COLORREF DarkMode_GetTextColor(void);

/* Dark background color */
COLORREF DarkMode_GetBgColor(void);

/* Handle WM_CTLCOLORSTATIC for dark static controls */
BOOL DarkMode_OnCtlStatic(HDC hdc, COLORREF* textColor);

/* Handle WM_CTLCOLORBTN for dark button area */
BOOL DarkMode_OnCtlButton(HDC hdc);

/* Dark colors */
#define DM_BG         0x002E2E2E
#define DM_BG_LIGHTER 0x003C3C3C
#define DM_TEXT       0x00FFFFFF
#define DM_ACCENT     0x00C8602C

#endif /* DARKMODE_H */
