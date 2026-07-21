#include "main.h"

const int EB_INTERVALS[EB_INTERVALS_COUNT] = {0, 1, 2, 4, 7, 15, 30};

/* --- Date helpers --- */
static const int DAYS_IN_MONTH[] = {0, 31,28,31,30,31,30,31,31,30,31,30,31};

static int is_leap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days_in_month(int y, int m) {
    if (m == 2 && is_leap(y)) return 29;
    return DAYS_IN_MONTH[m];
}

/* Convert (year, month, day) → days since EPOCH */
int DateToDays(int year, int month, int day) {
    int days = 0;
    for (int y = EPOCH_YEAR; y < year; y++)
        days += is_leap(y) ? 366 : 365;
    for (int m = 1; m < month; m++)
        days += days_in_month(year, m);
    days += day - 1;
    return days;
}

/* Convert days since EPOCH → (year, month, day) */
void DaysToDate(int days, int* year, int* month, int* day) {
    *year = EPOCH_YEAR;
    while (1) {
        int dy = is_leap(*year) ? 366 : 365;
        if (days < dy) break;
        days -= dy;
        (*year)++;
    }
    *month = 1;
    while (1) {
        int dm = days_in_month(*year, *month);
        if (days < dm) break;
        days -= dm;
        (*month)++;
    }
    *day = days + 1;
}

int TodayDays(void) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return DateToDays(st.wYear, st.wMonth, st.wDay);
}

/* Get path to the directory containing the executable */
void GetExePath(wchar_t* buf, int buflen) {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    wcscpy(buf, exe);
    wchar_t* slash = wcsrchr(buf, L'\\');
    if (slash) *slash = L'\0';
}

/* Get %APPDATA%/DesktopWord/ */
void GetAppDataPath(wchar_t* buf, int buflen) {
    wchar_t appdata[MAX_PATH];
    /* Use SHGetFolderPathW for broader compatibility */
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata))) {
        GetExePath(buf, buflen);
        return;
    }
    _snwprintf(buf, buflen, L"%s\\DesktopWord", appdata);
    /* ensure directory exists */
    CreateDirectoryW(buf, NULL);
}
