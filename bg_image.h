#ifndef BG_IMAGE_H
#define BG_IMAGE_H
#include <windows.h>

/* C-compatible interface for background image loading (GDI+ internally) */

/* Load an image file (PNG/JPG/BMP/GIF). Returns TRUE on success. */
BOOL BgLoadImage(const wchar_t* path);

/* Free the currently loaded background image. */
void BgFreeImage(void);

/* Render the loaded image scaled to w×h into a 32-bit BGRA buffer.
 * bits must point to a buffer of w*h*4 bytes.
 * Returns TRUE on success. */
BOOL BgRenderToBits(BYTE* bits, int w, int h);

/* Get original image dimensions. Returns 0 if no image loaded. */
int  BgGetImageWidth(void);
int  BgGetImageHeight(void);

/* Check if an image is currently loaded. */
BOOL BgIsLoaded(void);

#endif /* BG_IMAGE_H */
