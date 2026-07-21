#include "main.h"
#include "bg_image.h"

/* BGRA pixel helpers */
static void FillRectBGRA(BYTE* bits, int w, int h,
                     BYTE r, BYTE g, BYTE b, BYTE a)
{
    for (int i = 0; i < w * h; i++) {
        bits[i*4 + 0] = b;
        bits[i*4 + 1] = g;
        bits[i*4 + 2] = r;
        bits[i*4 + 3] = a;
    }
}

/* After GDI draws text on the DIB, its alpha channel is 0.
 * This pass: for any pixel whose color differs from the pixel
 * below it (i.e. GDI modified it), set alpha to 255.
 * Also: for transparent bg, non-text pixels stay alpha=0. */
static void FixupAlpha(BYTE* bits, int w, int h,
                       BYTE bg_r, BYTE bg_g, BYTE bg_b, BYTE bg_a)
{
    for (int i = 0; i < w * h; i++) {
        BYTE* p = &bits[i*4];
        if (p[0] != bg_b || p[1] != bg_g || p[2] != bg_r || p[3] != bg_a) {
            /* This pixel was modified by GDI text drawing → make it visible */
            /* Blend: keep the color, set alpha based on brightness */
            int bright = (p[0] + p[1] + p[2]) / 3;
            if (bright > 5) {
                p[3] = 255;  /* text pixels fully opaque */
            } else {
                p[3] = (BYTE)(bright * 20); /* dim pixels slightly transparent */
            }
        }
    }
}

void RenderWordWindow(HWND hwnd, WordEntry** entries, int count)
{
    if (count <= 0) return;

    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right  - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    BYTE bg_r, bg_g, bg_b, bg_a;

    if (g_bg_type == BG_IMAGE && BgIsLoaded()) {
        /* Image background */
        bg_r = bg_g = bg_b = 0; bg_a = 255;
    } else {
        /* Transparent background */
        bg_r = bg_g = bg_b = 0; bg_a = 0;
    }

    /* Create 32-bit DIB */
    HDC hdcScreen = GetDC(NULL);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* pBits = NULL;
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS,
                                    (void**)&pBits, NULL, 0);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

    /* Fill background */
    if (g_bg_type == BG_IMAGE && BgIsLoaded()) {
        /* Render the background image into the DIB */
        BgRenderToBits(pBits, w, h);
        bg_r = pBits[0]; /* sample first pixel for fixup reference */
        bg_g = pBits[1];
        bg_b = pBits[2];
        bg_a = pBits[3];
    } else {
        /* Transparent: fill with zeros */
        FillRectBGRA(pBits, w, h, bg_r, bg_g, bg_b, bg_a);
    }

    /* === Draw text with GDI === */
    SetBkMode(hdcMem, TRANSPARENT);

    int padding = 20;
    int y_pos = padding;

    /* Shadow / main text colors */
    COLORREF clrShadow = RGB(10, 10, 10);
    COLORREF clrMain   = RGB(255, 255, 255);

    for (int i = 0; i < count; i++) {
        WordDef* d = &entries[i]->def;

        /* Only enlarge the English word itself when hovered;
           phonetic/translation stay at normal size and are not interactive. */
        int wf_size = (i == g_hovered_index) ? g_font_large + 8 : g_font_large;
        int sf_size = g_font_small;
        int textH = wf_size + 12;
        int subH  = sf_size * 2 + 10;

        /* Store position for click/hover hit-testing — word body only */
        g_word_pos_y[i] = y_pos;
        g_word_pos_h[i] = textH;

        /* --- Large font: English word --- */
        HFONT hFontWord = CreateFontW(
            wf_size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFontWord);

        /* Draw shadow (4 directions, 1px offset) for readability on any bg */
        SetTextColor(hdcMem, clrShadow);
        RECT rcT = {padding-1, y_pos-1, w-padding, y_pos+textH};
        DrawTextW(hdcMem, d->word, -1, &rcT, DT_LEFT|DT_TOP|DT_NOCLIP);
        rcT.left = padding+1; rcT.top = y_pos-1;
        DrawTextW(hdcMem, d->word, -1, &rcT, DT_LEFT|DT_TOP|DT_NOCLIP);
        rcT.left = padding-1; rcT.top = y_pos+1;
        DrawTextW(hdcMem, d->word, -1, &rcT, DT_LEFT|DT_TOP|DT_NOCLIP);
        rcT.left = padding+1;
        DrawTextW(hdcMem, d->word, -1, &rcT, DT_LEFT|DT_TOP|DT_NOCLIP);

        /* Draw main text (white) */
        SetTextColor(hdcMem, clrMain);
        rcT.left = padding; rcT.top = y_pos;
        DrawTextW(hdcMem, d->word, -1, &rcT, DT_LEFT|DT_TOP|DT_NOCLIP);

        SelectObject(hdcMem, hOldFont);
        DeleteObject(hFontWord);

        y_pos += textH;

        /* --- Small font: phonetic + translation --- */
        HFONT hFontSub = CreateFontW(
            sf_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        hOldFont = (HFONT)SelectObject(hdcMem, hFontSub);

        wchar_t subBuf[256];
        _snwprintf(subBuf, 256, L"%s  %s", d->phonetic, d->translation);

        /* Shadow for sub text */
        SetTextColor(hdcMem, clrShadow);
        RECT rcSub = {padding-1, y_pos-1, w-padding, y_pos+subH};
        DrawTextW(hdcMem, subBuf, -1, &rcSub, DT_LEFT|DT_TOP|DT_NOCLIP);
        rcSub.left = padding+1; rcSub.top = y_pos-1;
        DrawTextW(hdcMem, subBuf, -1, &rcSub, DT_LEFT|DT_TOP|DT_NOCLIP);
        rcSub.left = padding-1; rcSub.top = y_pos+1;
        DrawTextW(hdcMem, subBuf, -1, &rcSub, DT_LEFT|DT_TOP|DT_NOCLIP);
        rcSub.left = padding+1;
        DrawTextW(hdcMem, subBuf, -1, &rcSub, DT_LEFT|DT_TOP|DT_NOCLIP);

        /* Main sub text */
        SetTextColor(hdcMem, clrMain);
        rcSub.left = padding; rcSub.top = y_pos;
        DrawTextW(hdcMem, subBuf, -1, &rcSub, DT_LEFT|DT_TOP|DT_NOCLIP);

        SelectObject(hdcMem, hOldFont);
        DeleteObject(hFontSub);

        y_pos += subH;
    }

    /* Fix alpha channel after GDI drawing */
    FixupAlpha(pBits, w, h, bg_r, bg_g, bg_b, bg_a);

    /* Update layered window */
    POINT ptSrc = {0, 0};
    SIZE  sizeWnd = {w, h};
    BLENDFUNCTION blend;
    blend.BlendOp             = AC_SRC_OVER;
    blend.BlendFlags          = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat         = AC_SRC_ALPHA;

    POINT ptDst = {rc.left, rc.top};
    ClientToScreen(hwnd, &ptDst);

    UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd,
                        hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    /* Cleanup */
    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

void FadeInWindow(HWND hwnd) {
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void FadeOutWindow(HWND hwnd) {
    ShowWindow(hwnd, SW_HIDE);
}
