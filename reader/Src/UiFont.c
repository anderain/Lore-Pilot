/*
 * UiFont.c
 *
 * Renders the 6x8 FontHybird glyphs as horizontal runs with WinDrawLine /
 * WinEraseLine, so it works on Palm OS 2.0 (1.0 trap range only).
 */

#include <PalmOS.h>

#include "UiFont.h"
#include "FontHybird6x8.h"

void UiFontDrawBitmap8(Int16 x, Int16 y, const UInt8 *rows, Boolean inverse)
{
    UInt16 row;

    if (rows == 0) return;

    for (row = 0; row < 8; row++) {
        UInt8 bits = rows[row];
        Int16 sy = (Int16)(y + (Int16)row);
        UInt16 c;

        if (sy < 0 || sy >= kUiFontScreenH) continue;

        c = 0;
        while (c < 8) {
            UInt16 start;
            Int16 x0;
            Int16 x1;

            if ((bits & (UInt8)(0x80 >> c)) == 0) {
                c++;
                continue;
            }

            start = c;
            while (c < 8 && (bits & (UInt8)(0x80 >> c)) != 0) {
                c++;
            }

            x0 = (Int16)(x + (Int16)start);
            x1 = (Int16)(x + (Int16)c - 1);

            if (x1 < 0 || x0 >= kUiFontScreenW) continue;
            if (x0 < 0) x0 = 0;
            if (x1 >= kUiFontScreenW) x1 = (Int16)(kUiFontScreenW - 1);

            if (inverse) {
                WinEraseLine(x0, sy, x1, sy);
            } else {
                WinDrawLine(x0, sy, x1, sy);
            }
        }
    }
}

void UiFontDrawBitmap8Gray(Int16 x, Int16 y, const UInt8 *rows)
{
    UInt16 row;

    if (rows == 0) return;

    for (row = 0; row < 8; row++) {
        UInt8 bits = rows[row];
        Int16 sy = (Int16)(y + (Int16)row);
        UInt16 c;

        if (sy < 0 || sy >= kUiFontScreenH) continue;

        c = 0;
        while (c < 8) {
            UInt16 start;
            Int16 x0;
            Int16 x1;

            if ((bits & (UInt8)(0x80 >> c)) == 0) {
                c++;
                continue;
            }

            start = c;
            while (c < 8 && (bits & (UInt8)(0x80 >> c)) != 0) {
                c++;
            }

            x0 = (Int16)(x + (Int16)start);
            x1 = (Int16)(x + (Int16)c - 1);

            if (x1 < 0 || x0 >= kUiFontScreenW) continue;
            if (x0 < 0) x0 = 0;
            if (x1 >= kUiFontScreenW) x1 = (Int16)(kUiFontScreenW - 1);

            WinDrawGrayLine(x0, sy, x1, sy);
        }
    }
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
