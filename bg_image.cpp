#include <windows.h>
#include <gdiplus.h>
#include <stdio.h>
using namespace Gdiplus;

static Image*  g_pBgImage  = NULL;
static BOOL    g_bLoaded   = FALSE;

/* One-time GDI+ init (called from main) */
static ULONG_PTR g_gdiToken = 0;

extern "C" {

void BgFreeImage(void); /* forward declaration */

BOOL BgInitGdiPlus(void) {
    GdiplusStartupInput si;
    if (GdiplusStartup(&g_gdiToken, &si, NULL) == Ok) {
        return TRUE;
    }
    return FALSE;
}

void BgShutdownGdiPlus(void) {
    BgFreeImage();
    if (g_gdiToken) {
        GdiplusShutdown(g_gdiToken);
        g_gdiToken = 0;
    }
}

BOOL BgLoadImage(const wchar_t* path) {
    if (g_pBgImage) {
        delete g_pBgImage;
        g_pBgImage = NULL;
    }
    g_bLoaded = FALSE;

    g_pBgImage = new Image(path);
    if (g_pBgImage->GetLastStatus() != Ok) {
        delete g_pBgImage;
        g_pBgImage = NULL;
        return FALSE;
    }

    g_bLoaded = TRUE;
    return TRUE;
}

void BgFreeImage(void) {
    if (g_pBgImage) {
        delete g_pBgImage;
        g_pBgImage = NULL;
    }
    g_bLoaded = FALSE;
}

BOOL BgIsLoaded(void) {
    return g_bLoaded && g_pBgImage != NULL;
}

int BgGetImageWidth(void) {
    if (!g_pBgImage) return 0;
    return (int)g_pBgImage->GetWidth();
}

int BgGetImageHeight(void) {
    if (!g_pBgImage) return 0;
    return (int)g_pBgImage->GetHeight();
}

BOOL BgRenderToBits(BYTE* bits, int w, int h) {
    if (!g_pBgImage || !bits || w <= 0 || h <= 0) return FALSE;

    /* Create a GDI+ Bitmap in 32-bit ARGB format */
    Bitmap bmp(w, h, PixelFormat32bppARGB);
    if (bmp.GetLastStatus() != Ok) return FALSE;

    Graphics graphics(&bmp);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    /* Fill with transparent black first */
    graphics.Clear(Color(0, 0, 0, 0));

    /* Draw image scaled to fill the entire area (preserving aspect ratio can be an option) */
    graphics.DrawImage(g_pBgImage, 0, 0, w, h);

    /* Extract pixel data — GDI+ ARGB → our BGRA buffer */
    BitmapData bmpData;
    Rect rect(0, 0, w, h);
    if (bmp.LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bmpData) != Ok) {
        return FALSE;
    }

    BYTE* src = (BYTE*)bmpData.Scan0;
    int stride = bmpData.Stride;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            /* GDI+ ARGB: [B][G][R][A] */
            BYTE* s = src + y * stride + x * 4;
            BYTE* d = bits + (y * w + x) * 4;
            d[0] = s[0]; /* B */
            d[1] = s[1]; /* G */
            d[2] = s[2]; /* R */
            d[3] = s[3]; /* A */
        }
    }

    bmp.UnlockBits(&bmpData);
    return TRUE;
}

} /* extern "C" */
