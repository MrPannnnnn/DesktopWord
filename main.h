#ifndef MAIN_H
#define MAIN_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define APP_NAME        L"DesktopWord"
#define APP_TITLE       L"桌面单词"
#define WINDOW_CLASS    L"DesktopWordClass"
#define WM_TRAYICON     (WM_APP + 1)
#define WM_DESKTOPCHANGE (WM_APP + 2)

#define ID_TRAY_SHOW      1001
#define ID_TRAY_SETTINGS  1002
#define ID_TRAY_IMPORT    1003
#define ID_TRAY_STATS     1004
#define ID_TRAY_AUTOSTART 1005
#define ID_TRAY_EXIT      1006
#define ID_TRAY_WORDMGR   1007

#define ID_TIMER_REFRESH  2001
#define ID_TIMER_DETECT   2002
#define ID_TIMER_BOTTOM   2003

#define MAX_WORDS         2000
#define MAX_DISPLAY_COUNT 20
#define MAX_WORD_LEN      64
#define MAX_PHONETIC_LEN  64
#define MAX_TRANS_LEN     128
#define MAX_LINE_LEN      512

#define EPOCH_YEAR        2025
#define EPOCH_MONTH       1
#define EPOCH_DAY         1

/* Ebbinghaus intervals: 0=initial, 1,2,4,7,15,30 days */
#define EB_INTERVALS_COUNT  7
extern const int EB_INTERVALS[EB_INTERVALS_COUNT];

/* --- Word structures --- */

typedef struct {
    wchar_t word[MAX_WORD_LEN];
    wchar_t phonetic[MAX_PHONETIC_LEN];
    wchar_t translation[MAX_TRANS_LEN];
} WordDef;  /* static definition from words.txt */

typedef struct {
    int first_seen;     /* days since epoch, 0 = never */
    int review_count;   /* 0 = never reviewed */
    int next_review;    /* days since epoch, 0 = any time */
    int last_shown;     /* days since epoch, 0 = never */
    BOOL familiar;      /* TRUE = user marked as known, never show again */
} WordProgress;

typedef struct {
    WordDef     def;
    WordProgress prog;
} WordEntry;

/* --- Global state --- */
extern HINSTANCE   g_hInst;
extern HWND        g_hWndMain;      /* hidden controller window */
extern HWND        g_hWndWord;      /* desktop overlay window */
extern BOOL        g_desktop_visible;
extern BOOL        g_app_running;

extern WordEntry*  g_words;
extern int         g_word_count;
extern int         g_word_capacity;

/* currently displayed word positions (for click hit-testing) */
extern WordEntry*  g_displayed[MAX_DISPLAY_COUNT];
extern int         g_displayed_count;
extern int         g_word_pos_y[MAX_DISPLAY_COUNT];
extern int         g_word_pos_h[MAX_DISPLAY_COUNT];
extern int         g_hovered_index;    /* -1=none, 0..MAX_DISPLAY_COUNT-1=word index being hovered */
extern BOOL        g_timer_paused;     /* TRUE if refresh timer is paused by hover */

/* settings */
extern int  g_refresh_sec;      /* word refresh interval */
extern int  g_display_count;    /* 1..MAX_DISPLAY_COUNT words at a time */
extern int  g_font_large;       /* large font size (word) */
extern int  g_font_small;       /* small font size (phonetic/trans) */
extern BOOL g_auto_start;
extern BOOL g_hover_pause;      /* TRUE=hover pauses timer (default) */
extern int  g_bg_type;          /* 0=transparent, 1=image */
extern wchar_t g_bg_path[MAX_PATH];  /* image file path */
extern HWND g_hWndWordMgr;      /* word manager dialog handle (singleton) */

/* background types */
#define BG_TRANSPARENT 0
#define BG_IMAGE       1

/* --- Function declarations --- */

/* word_store.c */
int     LoadWords(const wchar_t* path);
int     LoadProgress(const wchar_t* path);
int     SaveProgress(const wchar_t* path);
void    SaveProgressDefault(void);
void    InitWordProgress(void);

/* familiar word persistence (familiar.dat) */
int     SaveFamiliarWords(const wchar_t* path);
int     AppendFamiliarWord(const wchar_t* path, const wchar_t* word);
int     LoadFamiliarWords(const wchar_t* path);
void    GetFamiliarPath(wchar_t* buf, int buflen);

/* scheduler.c */
int     PickWords(WordEntry** selected, int max_count);
void    UpdateWordProgress(WordEntry* entry);

/* desktop_embed.c */
void    KeepWindowAtBottom(HWND hwnd);
void    PositionOverlayWindow(HWND hwnd);
BOOL    IsDesktopVisible(void);

/* render.c */
void    RenderWordWindow(HWND hwnd, WordEntry** entries, int count);
void    FadeInWindow(HWND hwnd);
void    FadeOutWindow(HWND hwnd);

/* tray.c */
BOOL    CreateTrayIcon(HWND hwnd);
void    RemoveTrayIcon(void);
void    ShowTrayContextMenu(HWND hwnd, LPARAM lParam);
void    UpdateTrayStats(void);

/* settings.c */
void    LoadSettings(void);
void    SaveSettings(void);
void    ShowSettingsDialog(HWND hwndParent);

/* word_bank.c */
void    ShowWordManagerDialog(HWND hwndParent);

/* bg_image.cpp (C++ with extern "C") */
BOOL    BgInitGdiPlus(void);
void    BgShutdownGdiPlus(void);
BOOL    BgLoadImage(const wchar_t* path);
void    BgFreeImage(void);
BOOL    BgRenderToBits(BYTE* bits, int w, int h);
BOOL    BgIsLoaded(void);

/* helpers */
int     DateToDays(int year, int month, int day);
void    DaysToDate(int days, int* year, int* month, int* day);
int     TodayDays(void);
void    GetAppDataPath(wchar_t* buf, int buflen);
void    GetExePath(wchar_t* buf, int buflen);

#endif /* MAIN_H */
