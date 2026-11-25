/*
 * UiFont.c
 *
 * Renders the 6x8 FontHybird glyphs and the built-in 8x8 Chinese glyphs.
 *
 * Both go through ScreenBuf, so they are written as packed pixels into the
 * frame buffer and reach the screen with one flush. On a heap too full for
 * the buffer ScreenBuf falls back to horizontal runs of WinDrawLine, which
 * looks identical but costs one trap per run.
 */

#include <PalmOS.h>

#include "UiFont.h"
#include "ScreenBuf.h"
#include "FontHybird6x8.h"

void UiFontDrawBitmap8(Int16 x, Int16 y, const UInt8 *rows, Boolean inverse)
{
    if (rows == 0) return;

    ScreenBufBits(x, y, rows, 8, 8, 1,
                  inverse ? kScreenInkWhite : kScreenInkBlack);
}

void UiFontDrawBitmap8Gray(Int16 x, Int16 y, const UInt8 *rows)
{
    if (rows == 0) return;

    ScreenBufBits(x, y, rows, 8, 8, 1, kScreenInkGray);
}

Int16 UiFontWidth(const Char *text)
{
    if (text == 0) return 0;
    return (Int16)(StrLen(text) * kUiFontWidth);
}

void UiFontDrawGlyph(Int16 x, Int16 y, UInt8 ch, Boolean inverse)
{
    if (ch < 0x20 || ch > 0x7E) return;

    /* The FontHybird glyphs are 6 px wide with the low two bits of every row
     * already clear, so drawing a full 8px cell is identical to 6px and lets
     * this share UiFontDrawBitmap8 with the Chinese glyphs. */
    UiFontDrawBitmap8(x, y, &FONT_HYBIRD_6x8[(UInt16)ch * kUiFontHeight],
                      inverse);
}

void UiFontDrawText(Int16 x, Int16 y, const Char *text, Boolean inverse)
{
    Int16 cx = x;
    UInt16 i;

    if (text == 0) return;

    for (i = 0; text[i] != '\0'; i++) {
        UiFontDrawGlyph(cx, y, (UInt8)text[i], inverse);
        cx = (Int16)(cx + kUiFontWidth);
    }
}
