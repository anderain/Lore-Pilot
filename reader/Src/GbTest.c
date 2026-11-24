/*
 * GbTest.c
 *
 * GB2312 paged font + Qianziwen paging test for LorePilotReader.
 *
 * Target: Palm OS 2.0 (Pilot Professional, 160x160, 1MB).
 * Uses only Palm OS 1.0 APIs through GlyphFont.c.
 *
 * One page shows 40 GB2312 units (8 four-character lines with
 * punctuation), so page turns land on sentence boundaries:
 *   1x: 20 units per line, 2 lines
 *   2x: 10 units per line, 4 lines
 */

#include <PalmOS.h>

#include "GbTest.h"
#include "GlyphFont.h"
#include "LorePilotReader_Rsc.h"
#include "GbSamples.h"
#include "GbText.h"

#define kGbTestWidth  160
#define kGbTestHeight 160

#define kGbText1xPerLine 20
#define kGbText2xPerLine 10
#define kGbCharsPerPage  40

static GlyphFont gGbFont;
static Boolean   gGbFontReady = false;
static UInt16    gGbSampleOk = 0;
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

static void GbTestAppendHex8(Char *dst, const UInt8 *bytes)
{
    UInt16 i;

    for (i = 0; i < 8; i++) {
        GbTestAppendHex2(dst, bytes[i]);
    }
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

/* Page 1 starts mid-paragraph, so page 0 shows the demo opening; later pages
   follow the 40-unit stride. Text is 1250 units => 32 pages. */

static UInt16 GbTestCodeAt(UInt16 index)
{
    if ((UInt16)(index * 2 + 1) >= (UInt16)kGbTextBytesCount) return 0;
    return (UInt16)(((UInt16)kGbTextBytes[index * 2] << 8) | kGbTextBytes[index * 2 + 1]);
}

/*------------------------------------------------------------------ load*/

static void GbTestLoad(void)
{
    UInt16 i;
    UInt8 rows[8];
    UInt16 pages;

    if (gGbFontReady) {
        GlyphFontClose(&gGbFont);
        gGbFontReady = false;
    }

    MemSet(&gGbFont, sizeof(gGbFont), 0);
    gGbFontReady = GlyphFontOpenAny(&gGbFont);

    pages = GbTestPageCount();
    if (pages == 0) pages = 1;
    if (gGbPage >= pages) gGbPage = (UInt16)(pages - 1);

    gGbSampleOk = 0;
    if (!gGbFontReady) return;

    for (i = 0; i < (UInt16)kGbSampleCount; i++) {
        if (!GlyphFontGet(&gGbFont, (UInt16)kGbSampleCodes[i], rows)) {
            continue;
        }
        if (rows[0] == kGbSampleBytes[i][0] &&
            rows[1] == kGbSampleBytes[i][1] &&
            rows[2] == kGbSampleBytes[i][2] &&
            rows[3] == kGbSampleBytes[i][3] &&
            rows[4] == kGbSampleBytes[i][4] &&
            rows[5] == kGbSampleBytes[i][5] &&
            rows[6] == kGbSampleBytes[i][6] &&
            rows[7] == kGbSampleBytes[i][7]) {
            gGbSampleOk++;
        }
    }
}

/*------------------------------------------------------------------ draw*/

static void GbTestDrawStatus(void)
{
    Char line[80];

    line[0] = '\0';
    if (gGbFontReady) {
        GbTestAppendStr(line, gGbFont.name);
        GbTestAppendStr(line, " v");
        GbTestAppendInt(line, gGbFont.version);
        GbTestAppendStr(line, " ");
        GbTestAppendInt(line, gGbFont.glyphCount);
        GbTestAppendStr(line, "g ");
        GbTestAppendInt(line, gGbFont.pageCount);
        GbTestAppendStr(line, "p");
    } else {
        GbTestAppendStr(line, "NO DB");
    }
    GbTestDrawText(0, 12, line);

    line[0] = '\0';
    if (gGbFontReady) {
        GbTestAppendStr(line, "SMP ");
        GbTestAppendInt(line, gGbSampleOk);
        GbTestAppendStr(line, "/");
        GbTestAppendInt(line, (UInt16)kGbSampleCount);
        GbTestAppendStr(line, gGbSampleOk == (UInt16)kGbSampleCount ? " OK " : " NG ");
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
    UInt8 rows[8];
    UInt16 code;

    line[0] = '\0';
    if (gGbSelected >= 0 && (UInt16)gGbSelected < GbTestCharCount()) {
        code = GbTestCodeAt((UInt16)gGbSelected);
        GbTestAppendStr(line, "GB=");
        GbTestAppendHex4(line, code);
        GbTestAppendStr(line, " ");
        if (gGbFontReady && GlyphFontGet(&gGbFont, code, rows)) {
            GbTestAppendHex8(line, rows);
        } else {
            GbTestAppendStr(line, "blank");
        }
    } else {
        GbTestAppendStr(line, "tap a glyph to inspect");
    }
    GbTestDrawText(0, 132, line);
}

static void GbTestDrawPage(Int16 y, UInt8 scale, UInt16 perLine)
{
    UInt16 start;
    UInt16 total;
    UInt16 i;
    UInt8 rows[8];

    if (!gGbFontReady) return;

    start = (UInt16)(gGbPage * kGbCharsPerPage);
    total = GbTestCharCount();

    for (i = 0; i < kGbCharsPerPage; i++) {
        UInt16 index = (UInt16)(start + i);
        UInt16 code;

        if (index >= total) break;
        code = GbTestCodeAt(index);
        if (GlyphFontGet(&gGbFont, code, rows)) {
            Int16 x = (Int16)((i % perLine) * 8 * scale);
            Int16 yy = (Int16)(y + (i / perLine) * 8 * scale);
            GlyphFontDrawGlyph(x, yy, rows, scale);
        }
    }
}

static void GbTestDrawAll(void)
{
    FntSetFont(stdFont);
    GbTestClearBody();

    GbTestDrawText(0, 2, "GB2312 Test");
    GbTestDrawStatus();

    if (gGbFontReady) {
        GbTestDrawPage(39, 1, kGbText1xPerLine);
        GbTestDrawPage(58, 2, kGbText2xPerLine);
    }

    GbTestDrawInfo();
}

/*------------------------------------------------------------------ events*/

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
                        GbTestDrawAll();
                    }
                    handled = true;
                    break;

                case GbTestNextButton:
                    if ((UInt16)(gGbPage + 1) < GbTestPageCount()) {
                        gGbPage++;
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
            /* 1x block: y 39..54; 2x block: y 58..121 */
            {
                UInt16 index = 0xFFFF;

                if (eventP->screenY >= 38 && eventP->screenY < 55 &&
                    eventP->screenX >= 0) {
                    UInt16 row = (UInt16)((eventP->screenY - 39) / 8);
                    UInt16 col = (UInt16)(eventP->screenX / 8);
                    index = (UInt16)(row * kGbText1xPerLine + col);
                } else if (eventP->screenY >= 57 && eventP->screenY < 123 &&
                           eventP->screenX >= 0) {
                    UInt16 row = (UInt16)((eventP->screenY - 58) / 16);
                    UInt16 col = (UInt16)(eventP->screenX / 16);
                    index = (UInt16)(row * kGbText2xPerLine + col);
                }

                if (index != 0xFFFF) {
                    UInt16 start = (UInt16)(gGbPage * kGbCharsPerPage);
                    if (index < kGbCharsPerPage &&
                        (UInt16)(start + index) < GbTestCharCount()) {
                        gGbSelected = (Int16)(start + index);
                        GbTestDrawAll();
                        handled = true;
                    }
                }
            }
            break;

        default:
            break;
    }

    return handled;
}
