/*
 * GbTest.c
 *
 * GB2312 paged font + Qianziwen paging test for LorePilotReader.
 *
 * Target: Palm OS 2.0 (Pilot Professional, 160x160, 1MB).
 * Uses only Palm OS 1.0 APIs through GlyphFont.c.
 *
 * One page shows 40 GB2312 units (8 four-character lines with punctuation),
 * so page turns land on sentence boundaries. Layout adapts to the cell size:
 *   8x8 : 1x 20 per line x 2 lines, 2x 10 per line x 4 lines
 *   12x12: 1x 13 per line x 4 lines, 2x 6 per line x 2 lines (12 of 40 shown)
 */

#include <PalmOS.h>

#include "GbTest.h"
#include "GlyphFont.h"
#include "LorePilotReader_Rsc.h"
#include "GbSamples.h"
#include "GbSamples12.h"
#include "GbText.h"

#define kGbTestWidth  160
#define kGbTestHeight 160
#define kGbCharsPerPage 40

static GlyphFont gGbFont;
static Boolean   gGbFontReady = false;
static UInt8     gGbCell = 8;
static UInt16    gGbSampleOk = 0;
static UInt16    gGbSampleTotal = 0;
static Int16     gGbSelected = -1;
static UInt16    gGbPage = 0;

/*------------------------------------------------------------------ helpers*/

static void GbTestAppendStr(Char *dst, const Char *src)
{
    StrCat(dst, src);
}

static void GbTestAppendInt(Char *dst, UInt16 value)
{
    Char tmp[8];

    StrIToA(tmp, (Int32)value);
    StrCat(dst, tmp);
}

static void GbTestAppendHex2(Char *dst, UInt8 value)
{
    static const Char hex[] = "0123456789ABCDEF";
    Char tmp[3];

    tmp[0] = hex[(value >> 4) & 0x0F];
    tmp[1] = hex[value & 0x0F];
    tmp[2] = '\0';
    StrCat(dst, tmp);
}

static void GbTestAppendHex4(Char *dst, UInt16 value)
{
    GbTestAppendHex2(dst, (UInt8)(value >> 8));
    GbTestAppendHex2(dst, (UInt8)(value & 0xFF));
}

static void GbTestDrawText(Int16 x, Int16 y, const Char *s)
{
    WinDrawChars(s, StrLen(s), x, y);
}

static void GbTestClearBody(void)
{
    RectangleType r;

    r.topLeft.x = 0;
    r.topLeft.y = 0;
    r.extent.x = kGbTestWidth;
    r.extent.y = 144;
    WinEraseRectangle(&r, 0);
}

static UInt16 GbTestCharCount(void)
{
    return (UInt16)(kGbTextBytesCount / 2);
}

static UInt16 GbTestPageCount(void)
{
    UInt16 count = GbTestCharCount();

    return (UInt16)((count + kGbCharsPerPage - 1) / kGbCharsPerPage);
}

static UInt16 GbTestCodeAt(UInt16 index)
{
    if ((UInt16)(index * 2 + 1) >= (UInt16)kGbTextBytesCount) return 0;
    return (UInt16)(((UInt16)kGbTextBytes[index * 2] << 8) | kGbTextBytes[index * 2 + 1]);
}

/*------------------------------------------------------------------ load*/

void GbTestRequest(UInt8 cell)
{
    if (cell == 8 || cell == 12) {
        gGbCell = cell;
    }
}

static Boolean GbTestCompareSample(const UInt8 *got, const UInt8 *want, UInt16 bytes)
{
    UInt16 i;

    for (i = 0; i < bytes; i++) {
        if (got[i] != want[i]) return false;
    }
    return true;
}

static void GbTestLoad(void)
{
    UInt16 i;
    UInt8 rows[kGlyphFontMaxGlyphBytes];
    UInt16 pages;
    const unsigned short *codes;

    if (gGbFontReady) {
        GlyphFontClose(&gGbFont);
        gGbFontReady = false;
    }

    MemSet(&gGbFont, sizeof(gGbFont), 0);
    gGbFontReady = GlyphFontFind(&gGbFont, gGbCell);

    pages = GbTestPageCount();
    if (pages == 0) pages = 1;
    if (gGbPage >= pages) gGbPage = (UInt16)(pages - 1);

    gGbSampleOk = 0;
    gGbSampleTotal = 0;
    if (!gGbFontReady) return;

    if (gGbCell == 12) {
        codes = kGbSampleCodes12;
        gGbSampleTotal = (UInt16)kGbSampleCount12;
        for (i = 0; i < gGbSampleTotal; i++) {
            if (!GlyphFontGet(&gGbFont, codes[i], rows)) continue;
            if (GbTestCompareSample(rows, kGbSampleBytes12[i], (UInt16)kGbSampleBytesPerGlyph12)) {
                gGbSampleOk++;
            }
        }
    } else {
        codes = kGbSampleCodes;
        gGbSampleTotal = (UInt16)kGbSampleCount;
        for (i = 0; i < gGbSampleTotal; i++) {
            if (!GlyphFontGet(&gGbFont, codes[i], rows)) continue;
            if (GbTestCompareSample(rows, kGbSampleBytes[i], (UInt16)kGbSampleBytesPerGlyph)) {
                gGbSampleOk++;
            }
        }
    }
}

/*------------------------------------------------------------------ layout*/

typedef struct GbLayoutType {
    UInt16 perLine;
    UInt16 lineCount;
    Int16  y;
    UInt8  scale;
    UInt16 count;
} GbLayout;

static void GbTestLayout(GbLayout *l1, GbLayout *l2)
{
    if (gGbFontReady && gGbFont.cellW == 12) {
        /* 40 units: 13+13+13+1 at 1x (y 36..84), then 12 units at 2x
           (6 per line, 24px tall lines, y 84..132). */
        l1->perLine = 13; l1->lineCount = 4; l1->y = 36; l1->scale = 1; l1->count = 40;
        l2->perLine = 6;  l2->lineCount = 2; l2->y = 84; l2->scale = 2; l2->count = 12;
    } else {
        l1->perLine = 20; l1->lineCount = 2; l1->y = 39; l1->scale = 1; l1->count = 40;
        l2->perLine = 10; l2->lineCount = 4; l2->y = 58; l2->scale = 2; l2->count = 40;
    }
}

/* Info line sits just above the button row; 12x12 blocks are taller. */
static Int16 GbTestInfoY(void)
{
    return 136;
}

/*------------------------------------------------------------------ draw*/

static void GbTestDrawStatus(void)
{
    Char line[80];

    line[0] = '\0';
    if (gGbFontReady) {
        GbTestAppendStr(line, gGbFont.name);
        GbTestAppendStr(line, " ");
        GbTestAppendInt(line, gGbFont.cellW);
        GbTestAppendStr(line, "x");
        GbTestAppendInt(line, gGbFont.cellH);
        GbTestAppendStr(line, " ");
        GbTestAppendInt(line, gGbFont.glyphCount);
        GbTestAppendStr(line, "g ");
        GbTestAppendInt(line, gGbFont.pageCount);
        GbTestAppendStr(line, "p");
    } else {
        GbTestAppendStr(line, "NO FONT ");
        GbTestAppendInt(line, (UInt16)gGbCell);
        GbTestAppendStr(line, "x");
        GbTestAppendInt(line, (UInt16)gGbCell);
    }
    GbTestDrawText(0, 12, line);

    line[0] = '\0';
    if (gGbFontReady) {
        GbTestAppendStr(line, "SMP ");
        GbTestAppendInt(line, gGbSampleOk);
        GbTestAppendStr(line, "/");
        GbTestAppendInt(line, gGbSampleTotal);
        GbTestAppendStr(line, gGbSampleOk == gGbSampleTotal ? " OK " : " NG ");
        GbTestAppendHex4(line, gGbFont.firstRow);
        GbTestAppendStr(line, "-");
        GbTestAppendHex4(line, gGbFont.lastRow);
    } else {
        GbTestAppendStr(line, "Install GB2312 pdb");
    }
    GbTestDrawText(0, 21, line);

    line[0] = '\0';
    GbTestAppendStr(line, "P ");
    GbTestAppendInt(line, (UInt16)(gGbPage + 1));
    GbTestAppendStr(line, "/");
    GbTestAppendInt(line, GbTestPageCount());
    {
        UInt16 start = (UInt16)(gGbPage * kGbCharsPerPage);
        UInt16 end = (UInt16)(start + kGbCharsPerPage);
        UInt16 total = GbTestCharCount();
        if (end > total) end = total;
        GbTestAppendStr(line, "  ");
        GbTestAppendInt(line, (UInt16)(start + 1));
        GbTestAppendStr(line, "-");
        GbTestAppendInt(line, end);
    }
    GbTestDrawText(0, 30, line);
}

static void GbTestDrawInfo(void)
{
    Char line[80];
    UInt8 rows[kGlyphFontMaxGlyphBytes];
    UInt16 code;
    UInt16 i;
    UInt16 bytes = 8;

    line[0] = '\0';
    if (gGbSelected >= 0 && (UInt16)gGbSelected < GbTestCharCount()) {
        code = GbTestCodeAt((UInt16)gGbSelected);
        GbTestAppendStr(line, "GB=");
        GbTestAppendHex4(line, code);
        GbTestAppendStr(line, " ");
        if (gGbFontReady && GlyphFontGet(&gGbFont, code, rows)) {
            if (gGbFont.bytesPerGlyph < bytes) bytes = gGbFont.bytesPerGlyph;
            for (i = 0; i < bytes; i++) {
                GbTestAppendHex2(line, rows[i]);
            }
        } else {
            GbTestAppendStr(line, "blank");
        }
    } else {
        GbTestAppendStr(line, "tap a glyph to inspect");
    }
    GbTestDrawText(0, GbTestInfoY(), line);
}

static void GbTestDrawBlock(const GbLayout *layout)
{
    UInt16 start;
    UInt16 total;
    UInt16 i;
    UInt8 rows[kGlyphFontMaxGlyphBytes];
    UInt16 cellW = gGbFont.cellW;
    UInt16 cellH = gGbFont.cellH;

    if (!gGbFontReady) return;

    start = (UInt16)(gGbPage * kGbCharsPerPage);
    total = GbTestCharCount();

    for (i = 0; i < layout->count; i++) {
        UInt16 index = (UInt16)(start + i);
        UInt16 code;

        if (index >= total) break;
        code = GbTestCodeAt(index);
        if (GlyphFontGet(&gGbFont, code, rows)) {
            Int16 x = (Int16)((i % layout->perLine) * cellW * layout->scale);
            Int16 y = (Int16)(layout->y + (i / layout->perLine) * cellH * layout->scale);
            GlyphFontDrawGlyph(&gGbFont, x, y, rows, layout->scale);
        }
    }
}

static void GbTestDrawAll(void)
{
    GbLayout l1, l2;

    FntSetFont(stdFont);
    GbTestClearBody();

    GbTestDrawText(0, 2, "GB2312 Test");
    GbTestDrawStatus();

    GbTestLayout(&l1, &l2);
    if (gGbFontReady) {
        GbTestDrawBlock(&l1);
        GbTestDrawBlock(&l2);
    }

    GbTestDrawInfo();
}

/*------------------------------------------------------------------ events*/

/* Find the glyph under a tap, using the same layout as drawing. */
static Int16 GbTestHitTest(Int16 x, Int16 y)
{
    GbLayout l1, l2;
    GbLayout *blocks[2];
    UInt16 b;

    GbTestLayout(&l1, &l2);
    blocks[0] = &l1;
    blocks[1] = &l2;

    if (!gGbFontReady) return -1;

    for (b = 0; b < 2; b++) {
        GbLayout *l = blocks[b];
        UInt16 cellW = gGbFont.cellW;
        UInt16 cellH = gGbFont.cellH;
        UInt16 rows = (UInt16)((l->count + l->perLine - 1) / l->perLine);
        Int16 blockW = (Int16)(l->perLine * cellW * l->scale);
        Int16 blockH = (Int16)(rows * cellH * l->scale);
        Int16 top = l->y;

        if (x >= 0 && x < blockW && y >= top && y < (Int16)(top + blockH)) {
            UInt16 col = (UInt16)(x / (cellW * l->scale));
            UInt16 row = (UInt16)((y - top) / (cellH * l->scale));
            UInt16 index = (UInt16)(row * l->perLine + col);
            UInt16 start = (UInt16)(gGbPage * kGbCharsPerPage);

            if (col < l->perLine && index < l->count &&
                (UInt16)(start + index) < GbTestCharCount()) {
                return (Int16)(start + index);
            }
            return -1;
        }
    }
    return -1;
}

Boolean GbTestFormHandleEvent(EventType *eventP)
{
    Boolean handled = false;

    switch (eventP->eType) {
        case frmOpenEvent:
            FrmDrawForm(FrmGetActiveForm());
            GbTestLoad();
            GbTestDrawAll();
            handled = true;
            break;

        case frmUpdateEvent:
            FrmDrawForm(FrmGetActiveForm());
            GbTestDrawAll();
            handled = true;
            break;

        case frmCloseEvent:
            if (gGbFontReady) {
                GlyphFontClose(&gGbFont);
                gGbFontReady = false;
            }
            handled = true;
            break;

        case ctlSelectEvent:
            switch (eventP->data.ctlSelect.controlID) {
                case GbTestPrevButton:
                    if (gGbPage > 0) {
                        gGbPage--;
                        gGbSelected = -1;
                        GbTestDrawAll();
                    }
                    handled = true;
                    break;

                case GbTestNextButton:
                    if ((UInt16)(gGbPage + 1) < GbTestPageCount()) {
                        gGbPage++;
                        gGbSelected = -1;
                        GbTestDrawAll();
                    }
                    handled = true;
                    break;

                case GbTestLoadButton:
                    GbTestLoad();
                    GbTestDrawAll();
                    handled = true;
                    break;

                case GbTestBackButton:
                    if (gGbFontReady) {
                        GlyphFontClose(&gGbFont);
                        gGbFontReady = false;
                    }
                    FrmGotoForm(MainForm);
                    handled = true;
                    break;

                default:
                    break;
            }
            break;

        case penDownEvent:
            {
                Int16 hit = GbTestHitTest(eventP->screenX, eventP->screenY);
                if (hit >= 0) {
                    gGbSelected = hit;
                    GbTestDrawAll();
                    handled = true;
                }
            }
            break;

        default:
            break;
    }

    return handled;
}
