/*
 * ScreenBuf.c
 *
 * 160-pixel 1-bit frame buffer plus a one-call flush.
 *
 * Why a hand-rolled bitmap instead of an offscreen window: on Palm OS 2.0
 * there is no way to get at a bitmap's pixels (BmpGetBits and WinGetBitmap are
 * 3.5+), so composing into an offscreen window would mean poking at
 * WindowType, whose layout is only documented for 5.0. The other way round is
 * clean: the version-0 bitmap header is 16 bytes of documented 16-bit fields
 * and its pixels follow inline, so one allocation holds both and
 * WinDrawBitmap (a 1.0 trap) puts the result on the screen.
 *
 * Fast path: when the destination is byte aligned the glyph row is OR'd in as
 * whole bytes, so an 8x8 glyph costs 8 stores instead of the ~10 WinDrawLine
 * traps the old code needed per glyph.
 *
 * Everything degrades to direct screen drawing when the buffer cannot be
 * allocated, so the reader still works on a nearly full heap.
 */

#include <PalmOS.h>

#include "ScreenBuf.h"

/* Version-0 bitmap header: width, height, rowBytes, flags, then four reserved
 * words. The ROM reads a pixelSize of 0 (the first reserved word) as 1bpp, and
 * because the indirect flag is clear it expects the pixels to follow the
 * header in the same block. */
#define kScreenBufHdrWords 8
#define kScreenBufHdrBytes 16

static MemHandle gHandle = 0;
static UInt16   *gHeader = 0;   /* kScreenBufHdrWords words at the block start */
static UInt8    *gBits = 0;     /* gHeader + kScreenBufHdrBytes                */
static UInt16    gHeight = 0;
static Boolean   gActive = false;

/*------------------------------------------------------------------ buffer*/

void ScreenBufFree(void)
{
    if (gHandle != 0) {
        if (gBits != 0) {
            MemHandleUnlock(gHandle);
        }
        MemHandleFree(gHandle);
    }
    gHandle = 0;
    gHeader = 0;
    gBits = 0;
    gActive = false;
    gHeight = 0;
}

Boolean ScreenBufAlloc(UInt16 height)
{
    MemHandle h;
    UInt8 *p;
    UInt16 i;

    if (height == 0 || height > kScreenH) height = kScreenH;

    if (gActive && gHeight == height) return true;

    ScreenBufFree();

    h = MemHandleNew((UInt32)kScreenBufHdrBytes +
                     (UInt32)height * kScreenRowBytes);
    if (h == 0) return false;

    p = (UInt8 *)MemHandleLock(h);
    if (p == 0) {
        MemHandleFree(h);
        return false;
    }

    gHandle = h;
    gHeader = (UInt16 *)p;
    gBits = p + kScreenBufHdrBytes;
    gHeight = height;
    gActive = true;

    for (i = 0; i < kScreenBufHdrWords; i++) gHeader[i] = 0;
    gHeader[0] = kScreenW;                  /* width                    */
    gHeader[1] = height;                    /* height                   */
    gHeader[2] = kScreenRowBytes;           /* rowBytes                 */
    gHeader[3] = 0;                         /* flags: raw, no colortable */

    MemSet(gBits, (UInt32)height * kScreenRowBytes, 0);
    return true;
}

/*------------------------------------------------------------------ pixels*/

static void SBMaskByte(UInt8 *p, UInt8 mask, ScreenInk ink)
{
    if (ink == kScreenInkBlack) {
        *p = (UInt8)(*p | mask);
    } else {
        *p = (UInt8)(*p & (UInt8)~mask);
    }
}

/* One horizontal run, clipped, in either mode. */
static void SBRun(Int16 y, Int16 x0, Int16 x1, ScreenInk ink)
{
    Int16 swap;
    UInt16 limit = gActive ? gHeight : (UInt16)kScreenH;

    if (y < 0 || (UInt16)y >= limit) return;

    if (x0 > x1) {
        swap = x0;
        x0 = x1;
        x1 = swap;
    }
    if (x0 < 0) x0 = 0;
    if (x1 > kScreenW - 1) x1 = (Int16)(kScreenW - 1);
    if (x0 > x1) return;

    if (!gActive) {
        if (ink == kScreenInkBlack) {
            WinDrawLine(x0, y, x1, y);
        } else if (ink == kScreenInkWhite) {
            WinEraseLine(x0, y, x1, y);
        } else {
            WinDrawGrayLine(x0, y, x1, y);
        }
        return;
    }

    {
        UInt8 *line = gBits + (UInt32)(UInt16)y * kScreenRowBytes;

        if (ink == kScreenInkGray) {
            /* 50% checkerboard; only small captions use it. */
            Int16 x;

            for (x = x0; x <= x1; x++) {
                UInt8 bit = (UInt8)(0x80 >> (x & 7));

                if ((((UInt16)x + (UInt16)y) & 1) != 0) {
                    line[x >> 3] = (UInt8)(line[x >> 3] | bit);
                } else {
                    line[x >> 3] = (UInt8)(line[x >> 3] & (UInt8)~bit);
                }
            }
            return;
        }

        {
            UInt16 lo = (UInt16)(x0 & 7);    /* first bit in the first byte */
            UInt16 hi = (UInt16)(x1 & 7);    /* last  bit in the last  byte  */
            UInt16 b0 = (UInt16)((UInt16)x0 >> 3);
            UInt16 b1 = (UInt16)((UInt16)x1 >> 3);
            UInt16 b;

            if (b0 == b1) {
                SBMaskByte(line + b0,
                           (UInt8)((0xFFu >> lo) &
                                   (UInt8)(0xFFu << (7 - hi))),
                           ink);
                return;
            }

            SBMaskByte(line + b0, (UInt8)(0xFFu >> lo), ink);
            for (b = (UInt16)(b0 + 1); b < b1; b++) {
                SBMaskByte(line + b, 0xFF, ink);
            }
            SBMaskByte(line + b1, (UInt8)(0xFFu << (7 - hi)), ink);
        }
    }
}

/*------------------------------------------------------------------ fills*/

void ScreenBufClearRect(UInt16 top, UInt16 rows)
{
    RectangleType r;

    if (rows == 0) return;

    if (!gActive) {
        r.topLeft.x = 0;
        r.topLeft.y = (Coord)top;
        r.extent.x = kScreenW;
        r.extent.y = (Coord)rows;
        WinEraseRectangle(&r, 0);
        return;
    }

    if (top >= gHeight) return;
    if ((UInt32)top + rows > gHeight) rows = (UInt16)(gHeight - top);
    MemSet(gBits + (UInt32)top * kScreenRowBytes,
           (UInt32)rows * kScreenRowBytes, 0);
}

void ScreenBufFillBand(UInt16 top, UInt16 rows)
{
    RectangleType r;

    if (rows == 0) return;

    if (!gActive) {
        r.topLeft.x = 0;
        r.topLeft.y = (Coord)top;
        r.extent.x = kScreenW;
        r.extent.y = (Coord)rows;
        WinDrawRectangle(&r, 0);
        return;
    }

    if (top >= gHeight) return;
    if ((UInt32)top + rows > gHeight) rows = (UInt16)(gHeight - top);
    MemSet(gBits + (UInt32)top * kScreenRowBytes,
           (UInt32)rows * kScreenRowBytes, 0xFF);
}

/*------------------------------------------------------------------ shapes*/

void ScreenBufHLine(Int16 y, Int16 x0, Int16 x1, ScreenInk ink)
{
    SBRun(y, x0, x1, ink);
}

/*------------------------------------------------------------------ glyphs*/

void ScreenBufBits(Int16 x, Int16 y, const UInt8 *rows,
                   UInt16 w, UInt16 h, UInt16 rowBytes, ScreenInk ink)
{
    UInt16 r;

    if (rows == 0 || w == 0 || h == 0 || rowBytes == 0) return;

    /* Byte-aligned, in range, opaque: OR whole bytes. This is the path every
     * 8x8 glyph takes, because the reader places cells at multiples of 8. */
    if (ink == kScreenInkBlack && gActive &&
        (x & 7) == 0 && x >= 0 && (UInt16)(x + (Int16)w) <= kScreenW &&
        (w & 7) == 0 && rowBytes == (UInt16)(w >> 3)) {
        UInt16 dstBytes = (UInt16)(w >> 3);
        UInt16 baseCol = (UInt16)((UInt16)x >> 3);

        for (r = 0; r < h; r++) {
            Int16 sy = (Int16)(y + (Int16)r);
            UInt8 *dst;
            UInt16 c;

            if (sy < 0 || (UInt16)sy >= gHeight) continue;
            dst = gBits + (UInt32)(UInt16)sy * kScreenRowBytes + baseCol;
            for (c = 0; c < dstBytes; c++) {
                dst[c] = (UInt8)(dst[c] | rows[(UInt32)r * rowBytes + c]);
            }
        }
        return;
    }

    for (r = 0; r < h; r++) {
        const UInt8 *row = rows + (UInt32)r * rowBytes;
        UInt16 c = 0;

        while (c < w) {
            UInt16 start;

            if ((row[c >> 3] & (UInt8)(0x80 >> (c & 7))) == 0) {
                c++;
                continue;
            }

            start = c;
            while (c < w && (row[c >> 3] & (UInt8)(0x80 >> (c & 7))) != 0) {
                c++;
            }

            SBRun((Int16)(y + (Int16)r),
                  (Int16)(x + (Int16)start),
                  (Int16)(x + (Int16)c - 1), ink);
        }
    }
}

void ScreenBufCell(Int16 x, Int16 y, const UInt8 *rows, UInt16 cell,
                   ScreenInk ink)
{
    if (rows == 0 || cell == 0) return;
    ScreenBufBits(x, y, rows, cell, cell, (UInt16)((cell + 7) >> 3), ink);
}

/*------------------------------------------------------------------ flush*/

void ScreenBufFlush(void)
{
    if (!gActive) return;
    WinDrawBitmap((BitmapPtr)gHeader, 0, 0);
}
