/*
 * ScreenBuf.h
 *
 * 160-pixel-wide 1-bit frame buffer.
 *
 * Everything is composed in memory and put on the screen with a single
 * WinDrawBitmap call, so a page turn never shows the blank frame that
 * "erase the screen, then draw into it" produces.
 *
 * Every call has a direct-drawing fallback (WinDrawLine and friends) for when
 * the buffer cannot be allocated, so callers never need to know which mode is
 * active.
 *
 * Palm OS 2.0 safe: WinDrawBitmap is a 1.0 trap (0xA226). The 3.5+ calls that
 * would expose a bitmap's pixels (BmpGetBits) are deliberately not used: the
 * bitmap header is written by hand instead.
 */

#ifndef SCREENBUF_H_
#define SCREENBUF_H_

#include <PalmOS.h>

#define kScreenW         160
#define kScreenH         160
#define kScreenRowBytes  (kScreenW / 8)

/* How a 1 bit in the source is transferred. White clears the destination bit,
 * which is how text is drawn into the black bands. Clearing a bit never
 * touches a neighbouring pixel. */
typedef enum ScreenInkTag {
    kScreenInkBlack = 0,
    kScreenInkWhite,
    kScreenInkGray
} ScreenInk;

/* Allocate a `height`-row frame buffer. Returns false when the heap is too
 * full; drawing then falls back to the screen. Calling it again with the same
 * height is a no-op, so a form can call it defensively from every redraw. */
Boolean ScreenBufAlloc(UInt16 height);

/* Release the frame buffer. */
void ScreenBufFree(void);

/* Whitewash rows [top, top + rows). */
void ScreenBufClearRect(UInt16 top, UInt16 rows);

/* Fill rows [top, top + rows) with black. */
void ScreenBufFillBand(UInt16 top, UInt16 rows);

/* Horizontal line, clipped to the buffer. */
void ScreenBufHLine(Int16 y, Int16 x0, Int16 x1, ScreenInk ink);

/* Blit a bit-packed source (MSB first, 1 = ink) with clipping. */
void ScreenBufBits(Int16 x, Int16 y, const UInt8 *rows,
                   UInt16 w, UInt16 h, UInt16 rowBytes, ScreenInk ink);

/* Blit one square glyph cell (8x8 or 12x12). */
void ScreenBufCell(Int16 x, Int16 y, const UInt8 *rows, UInt16 cell,
                   ScreenInk ink);

/* Copy the finished frame to the screen. No-op in fallback mode. */
void ScreenBufFlush(void);

#endif /* SCREENBUF_H_ */
