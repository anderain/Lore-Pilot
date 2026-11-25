/*
 * GbTest.c
 *
 * GB2312 paged font + Qianziwen paging test for LorePilotReader.
 *
 * Target: Palm OS 2.0 (Pilot Professional, 160x160, 1MB).
 *
 * One page shows 40 GB2312 units, so page turns land on sentence boundaries.
 * The glyph block is drawn at 1x only: the old 2x copy existed to exercise the
 * scaled run drawing, which the reader never used. Glyphs go into the frame
 * buffer and reach the screen with one flush; the status lines are drawn with
 * the system font on top of it.
 *
 *   8x8 : 20 per line x 2 lines
 *   12x12: 13 per line x 4 lines (40th unit on its own line)
 */

#include <PalmOS.h>

#include "GbTest.h"
#include "GlyphFont.h"
#include "ScreenBuf.h"
#include "LorePilotReader_Rsc.h"
#include "GbSamples.h"
#include "GbSamples12.h"
#include "GbText.h"

#define kGbCharsPerPage 40

/* Body area above the form's buttons (they start at y 146). The status lines
 * use y 12, 21 and 30 and the system font is ~11 px tall, so the glyph block
 * starts at 44 and the info line sits at 132 (11 px tall, clears y 146). */
#define kGbTestBodyH    146
#define kGbTestInfoY    132
#define kGbTestBlockTop 44

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

/* Filled from the font so drawing and hit testing cannot drift apart. */
typedef struct GbLayoutType {
    UInt16 perLine;
    UInt16 lineCount;
    Int16  y;
    UInt16 count;
} GbLayout;

static void GbTestLayout(GbLayout *l)
{
    if (gGbFontReady && gGbFont.cellW == 12) {
        l->perLine = 13;
        l->lineCount = 4;
    } else {
        l->perLine = 20;
        l->lineCount = 2;
    }
    l->y = kGbTestBlockTop;
    l->count = kGbCharsPerPage;
}

/*------------------------------------------------------------------ draw*/

/* Status lines are ASCII with the system font; they are drawn after the
 * frame buffer flush so they land on top of the composed page. */
static void GbTestDrawStatus(void)
{
    Char line[80];

    FntSetFont(stdFont);

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

    FntSetFont(stdFont);

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
    GbTestDrawText(0, kGbTestInfoY, line);
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
            Int16 x = (Int16)((i % layout->perLine) * cellW);
            Int16 y = (Int16)(layout->y + (i / layout->perLine) * cellH);
            GlyphFontDrawGlyph(&gGbFont, x, y, rows);
        }
    }
}

static void GbTestDrawAll(void)
{
    GbLayout layout;

    (void)ScreenBufAlloc(kGbTestBodyH);
    ScreenBufClearRect(0, kGbTestBodyH);

    GbTestLayout(&layout);
    if (gGbFontReady) {
        GbTestDrawBlock(&layout);
    }

    /* One flush, then the ASCII status on top of the composed page. */
    ScreenBufFlush();

    GbTestDrawStatus();
    GbTestDrawInfo();
}

/*------------------------------------------------------------------ events*/

/* Find the glyph under a tap, using the same layout as drawing. */
static Int16 GbTestHitTest(Int16 x, Int16 y)
{
    GbLayout layout;
    UInt16 cellW;
    UInt16 cellH;
    UInt16 rows;
    Int16 blockW;
    Int16 blockH;

    if (!gGbFontReady) return -1;

    GbTestLayout(&layout);

    cellW = gGbFont.cellW;
    cellH = gGbFont.cellH;
    rows = (UInt16)((layout.count + layout.perLine - 1) / layout.perLine);
    blockW = (Int16)(layout.perLine * cellW);
    blockH = (Int16)(rows * cellH);

    if (x >= 0 && x < blockW && y >= layout.y &&
        y < (Int16)(layout.y + blockH)) {
        UInt16 col = (UInt16)(x / cellW);
        UInt16 row = (UInt16)((y - layout.y) / cellH);
        UInt16 index = (UInt16)(row * layout.perLine + col);
        UInt16 start = (UInt16)(gGbPage * kGbCharsPerPage);

        if (col < layout.perLine && index < layout.count &&
            (UInt16)(start + index) < GbTestCharCount()) {
            return (Int16)(start + index);
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
            ScreenBufFree();
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
                    ScreenBufFree();
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
