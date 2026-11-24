/*
 * ReaderForm.c
 *
 * Full-screen page reader. Layout mirrors lore-teller/src/lib/render.ts.
 *
 *   y 0..7      black band, white 6x8 ASCII: displayName + "N/T"
 *   y 8..149    white body, glyph font, vertically centred
 *   y 150..157  black band, white 6x8 ASCII: PREV  MENU  NEXT
 *   y 158       white progress border
 *   y 159       white progress fill (160 * page / total)
 *
 * Files written by older lore-teller builds have progressRows == 0: the body
 * then runs 8..151 and the footer sits at 152..159, with no progress bar.
 *
 * Only Palm OS 1.0/2.x calls are used (WinDrawLine, WinEraseLine,
 * WinEraseRectangle, Data Manager 1.0).
 */

#include <PalmOS.h>

#include "ReaderForm.h"
#include "TextDb.h"
#include "BookList.h"
#include "GlyphFont.h"
#include "UiFont.h"
#include "LorePilotReader_Rsc.h"

#define kReaderW            160
#define kReaderH            160
#define kReaderDefaultHdr   8
#define kReaderDefaultFtr   8
#define kReaderProgressRows 2

/* Bottom tap strip, split into three equal columns (x 0..52 / 53..105 /
 * 106..159). It starts at y 148: below the last possible body glyph row in
 * both layouts (8px text ends at y 146), so no text is ever covered. */
#define kReaderTapTop       148
#define kReaderTapLeftEnd   53
#define kReaderTapRightFrom 106

static TextDb     gReaderDb;
static Boolean    gReaderOpen = false;
static TextDbPage gReaderPage;
static GlyphFont  gReaderFont;
static Boolean    gReaderFontReady = false;

static Char       gReaderName[17];
static UInt16     gReaderCell = 8;
static UInt16     gReaderPageIndex = 0;
static Boolean    gReaderRequested = false;

/*------------------------------------------------------------------ helpers*/

static void ReaderAppendStr(Char *dst, const Char *src, UInt16 max)
{
    UInt16 len = StrLen(dst);

    while (*src != '\0' && len < (UInt16)(max - 1)) {
        dst[len++] = *src++;
    }
    dst[len] = '\0';
}

static void ReaderAppendInt(Char *dst, UInt32 value, UInt16 max)
{
    Char tmp[12];

    StrIToA(tmp, (Int32)value);
    ReaderAppendStr(dst, tmp, max);
}

/*------------------------------------------------------------------ layout*/

static UInt16 ReaderBodyTop(void)
{
    UInt16 rows = gReaderDb.header.headerRows;

    if (rows == 0) rows = kReaderDefaultHdr;
    if (rows > 40) rows = kReaderDefaultHdr;
    return rows;
}

static UInt16 ReaderFooterRows(void)
{
    UInt16 rows = gReaderDb.header.footerRows;

    if (rows == 0) rows = kReaderDefaultFtr;
    if (rows > 40) rows = kReaderDefaultFtr;
    return rows;
}

static UInt16 ReaderProgressRows(void)
{
    UInt16 rows = gReaderDb.header.progressRows;

    if (rows > 4) rows = 0;
    return rows;
}

/* First row below the body band, i.e. the top of the footer band. */
static UInt16 ReaderBodyBottom(void)
{
    UInt16 rows = (UInt16)(ReaderFooterRows() + ReaderProgressRows());

    if (rows >= kReaderH) rows = kReaderDefaultFtr;
    return (UInt16)(kReaderH - rows);
}

static UInt16 ReaderCharsPerLine(void)
{
    UInt16 perLine = (UInt16)(kReaderW / gReaderCell);

    if (gReaderDb.header.charsPerLine > 0 &&
        gReaderDb.header.charsPerLine < perLine) {
        perLine = gReaderDb.header.charsPerLine;
    }
    return perLine;
}

static UInt32 ReaderGlobalPage(void)
{
    return gReaderDb.header.firstPage + gReaderPageIndex;
}

/*------------------------------------------------------------------ drawing*/

static void ReaderFillBand(UInt16 top, UInt16 rows)
{
    UInt16 y;

    for (y = 0; y < rows; y++) {
        UInt16 yy = (UInt16)(top + y);

        if (yy >= kReaderH) break;
        WinDrawLine(0, (Coord)yy, (Coord)(kReaderW - 1), (Coord)yy);
    }
}

/* Draw one body line: GB2312 double bytes, one glyph per cell. */
static void ReaderDrawLine(UInt16 x, UInt16 y, UInt16 start, UInt16 bytes)
{
    UInt16 perLine = ReaderCharsPerLine();
    UInt16 col;
    UInt8  rows[kGlyphFontMaxGlyphBytes];

    for (col = 0; col < perLine; col++) {
        UInt16 offset = (UInt16)(start + col * 2);
        UInt16 code;

        if ((UInt16)(col * 2 + 1) >= bytes) break;
        if (offset + 1 >= gReaderPage.textLen) break;

        code = (UInt16)(((UInt16)gReaderPage.text[offset] << 8) |
                        (UInt16)gReaderPage.text[offset + 1]);
        if (GlyphFontGet(&gReaderFont, code, rows)) {
            GlyphFontDrawGlyph(&gReaderFont,
                               (Int16)(x + col * gReaderCell),
                               (Int16)y, rows, 1);
        }
    }
}

static void ReaderDrawHeader(void)
{
    Char label[40];
    UInt16 len;
    Int16  x;

    ReaderFillBand(0, ReaderBodyTop());

    /* Left: display name, white on black. */
    UiFontDrawText(0, 0, gReaderDb.header.displayName, true);

    /* Right: global page / total pages. */
    label[0] = '\0';
    ReaderAppendInt(label, ReaderGlobalPage(), sizeof(label));
    ReaderAppendStr(label, "/", sizeof(label));
    ReaderAppendInt(label, gReaderDb.header.totalPages, sizeof(label));

    len = StrLen(label);
    x = (Int16)(kReaderW - (Int16)len * kUiFontWidth);
    if (x < 0) x = 0;
    UiFontDrawText(x, 0, label, true);
}

static void ReaderDrawFooter(void)
{
    UInt16 top = ReaderBodyBottom();
    UInt16 rows = ReaderFooterRows();
    UInt16 progressRows = ReaderProgressRows();
    UInt16 borderY;
    UInt16 fillY;
    UInt32 total;
    UInt32 page;
    UInt32 filled;

    /* Black band covers the text rows plus the progress rows. */
    ReaderFillBand(top, (UInt16)(rows + progressRows));

    UiFontDrawText(2, (Int16)top, "PREV", true);
    UiFontDrawText(74, (Int16)top, "MENU", true);
    UiFontDrawText(134, (Int16)top, "NEXT", true);

    if (progressRows < kReaderProgressRows) {
        return;
    }

    borderY = (UInt16)(top + rows);
    fillY = (UInt16)(borderY + 1);

    /* Border: full-width white line. */
    WinEraseLine(0, (Coord)borderY, kReaderW - 1, (Coord)borderY);

    /* Fill: white from x=0, length = round(160 * page / total). */
    total = gReaderDb.header.totalPages;
    if (total == 0) total = 1;
    page = ReaderGlobalPage();
    if (page > total) page = total;

    filled = (UInt32)kReaderW * page;
    filled = (filled + total / 2) / total;
    if (filled > (UInt32)kReaderW) filled = (UInt32)kReaderW;

    if (filled > 0 && fillY < kReaderH) {
        WinEraseLine(0, (Coord)fillY, (Coord)(filled - 1), (Coord)fillY);
    }
}

static void ReaderDrawBody(void)
{
    UInt16 top = ReaderBodyTop();
    UInt16 bottom = ReaderBodyBottom();
    UInt16 height;
    UInt16 maxLines;
    UInt16 used;
    UInt16 y;
    UInt16 i;
    RectangleType r;

    if (bottom <= top) return;
    height = (UInt16)(bottom - top);

    r.topLeft.x = 0;
    r.topLeft.y = (Coord)top;
    r.extent.x = kReaderW;
    r.extent.y = (Coord)height;
    WinEraseRectangle(&r, 0);

    if (!gReaderFontReady) {
        Char msg[24];

        msg[0] = '\0';
        ReaderAppendStr(msg, "NO FONT ", sizeof(msg));
        ReaderAppendInt(msg, gReaderCell, sizeof(msg));
        ReaderAppendStr(msg, "x", sizeof(msg));
        ReaderAppendInt(msg, gReaderCell, sizeof(msg));
        UiFontDrawText((Int16)((kReaderW - (Int16)StrLen(msg) * kUiFontWidth) / 2),
                       (Int16)(top + height / 2 - kUiFontHeight / 2),
                       msg, false);
        return;
    }

    maxLines = (UInt16)(height / gReaderCell);
    used = gReaderPage.lineCount;
    if (used > maxLines) used = maxLines;

    /* Vertically centre the used lines, as the web preview does. */
    y = (UInt16)(top + (height - used * gReaderCell) / 2);

    for (i = 0; i < used; i++) {
        ReaderDrawLine(0, (UInt16)(y + i * gReaderCell),
                       gReaderPage.lineStart[i], gReaderPage.lineLength[i]);
    }
}

static void ReaderDrawAll(void)
{
    RectangleType r;

    r.topLeft.x = 0;
    r.topLeft.y = 0;
    r.extent.x = kReaderW;
    r.extent.y = kReaderH;
    WinEraseRectangle(&r, 0);

    if (!gReaderOpen) return;

    ReaderDrawBody();
    ReaderDrawHeader();
    ReaderDrawFooter();
}

/*------------------------------------------------------------------ loading*/

static void ReaderFormClose(void);

/* Open part `partNumber` (1-based) and show `pageIndex`. The switch only
 * commits when the database opens, so a missing part leaves the reader on the
 * page it was showing. */
static Boolean ReaderLoadPart(UInt16 partNumber, UInt16 pageIndex)
{
    Char      dbName[32];
    TextDb    next;
    Boolean   ok;
    UInt16    page;

    dbName[0] = '\0';
    BookListPartName(gReaderName, partNumber, dbName, sizeof(dbName));

    if (!TextDbOpen(&next, dbName)) {
        return false;
    }

    if (pageIndex >= next.header.pageCount) {
        page = (UInt16)(next.header.pageCount - 1);
    } else {
        page = pageIndex;
    }

    ok = TextDbReadPage(&next, page, &gReaderPage);
    if (!ok) {
        MemSet(&gReaderPage, sizeof(gReaderPage), 0);
    }

    if (gReaderOpen) {
        TextDbClose(&gReaderDb);
    }
    gReaderDb = next;
    gReaderOpen = true;
    gReaderPageIndex = page;
    return true;
}

static Boolean ReaderNextPage(void)
{
    if (!gReaderOpen) return false;

    if ((UInt32)(gReaderPageIndex + 1) < gReaderDb.header.pageCount) {
        gReaderPageIndex++;
        if (!TextDbReadPage(&gReaderDb, gReaderPageIndex, &gReaderPage)) {
            MemSet(&gReaderPage, sizeof(gReaderPage), 0);
        }
        return true;
    }

    if ((UInt16)(gReaderDb.header.partIndex + 1) < gReaderDb.header.partCount) {
        return ReaderLoadPart((UInt16)(gReaderDb.header.partIndex + 2), 0);
    }
    return false;
}

static Boolean ReaderPrevPage(void)
{
    if (!gReaderOpen) return false;

    if (gReaderPageIndex > 0) {
        gReaderPageIndex--;
        if (!TextDbReadPage(&gReaderDb, gReaderPageIndex, &gReaderPage)) {
            MemSet(&gReaderPage, sizeof(gReaderPage), 0);
        }
        return true;
    }

    if (gReaderDb.header.partIndex > 0) {
        UInt16 prevPart = gReaderDb.header.partIndex;   /* 1-based number */

        /* Open the previous part at its last page. ReaderLoadPart clamps the
         * index to the page count, so pass a huge page number. */
        return ReaderLoadPart(prevPart, 0xFFFF);
    }
    return false;
}

static void ReaderOpenFont(void)
{
    if (gReaderFontReady) {
        GlyphFontClose(&gReaderFont);
        gReaderFontReady = false;
    }
    MemSet(&gReaderFont, sizeof(gReaderFont), 0);
    gReaderFontReady = GlyphFontFind(&gReaderFont, (UInt8)gReaderCell);
}

/*------------------------------------------------------------------ public*/

/* Store the book to open and load the first page. Called from the bookshelf
 * before switching forms, so no form transition happens inside an event
 * handler. Returns false if the first part could not be opened. */
Boolean ReaderFormRequest(const Char *displayName, UInt16 cell)
{
    UInt16 i = 0;

    gReaderRequested = false;
    gReaderName[0] = '\0';

    if (displayName == 0) return false;

    while (i < 16 && displayName[i] != '\0') {
        gReaderName[i] = displayName[i];
        i++;
    }
    gReaderName[i] = '\0';

    gReaderCell = (UInt16)((cell == 12) ? 12 : 8);

    if (!ReaderLoadPart(1, 0)) {
        /* Drop any book left open by an earlier session. */
        ReaderFormClose();
        return false;
    }

    /* Trust the cell size recorded in the database header. */
    gReaderCell = gReaderDb.header.cell;
    ReaderOpenFont();

    gReaderRequested = true;
    return true;
}

static void ReaderFormClose(void)
{
    if (gReaderFontReady) {
        GlyphFontClose(&gReaderFont);
        gReaderFontReady = false;
    }
    if (gReaderOpen) {
        TextDbClose(&gReaderDb);
        gReaderOpen = false;
    }
    gReaderRequested = false;
}

/* Bottom third-tap strip. */
static Boolean ReaderHandleTap(Int16 x, Int16 y)
{
    if (y < kReaderTapTop) return false;

    if (x <= kReaderTapLeftEnd) {
        if (ReaderPrevPage()) {
            ReaderDrawAll();
        }
        return true;
    }

    if (x < kReaderTapRightFrom) {
        /* Close the database before switching forms, like the test forms do:
         * frmCloseEvent delivery order is not guaranteed across ROMs. */
        ReaderFormClose();
        FrmGotoForm(MainForm);
        return true;
    }

    if (ReaderNextPage()) {
        ReaderDrawAll();
    }
    return true;
}

Boolean ReaderFormHandleEvent(EventType *eventP)
{
    Boolean handled = false;

    switch (eventP->eType) {
        case frmOpenEvent:
            FrmDrawForm(FrmGetActiveForm());
            if (gReaderRequested && gReaderOpen) {
                ReaderDrawAll();
            }
            handled = true;
            break;

        case frmUpdateEvent:
            if (gReaderOpen) {
                ReaderDrawAll();
            }
            handled = true;
            break;

        case frmCloseEvent:
            ReaderFormClose();
            handled = true;
            break;

        case penDownEvent:
            handled = ReaderHandleTap(eventP->screenX, eventP->screenY);
            break;

        case keyDownEvent:
            {
                UInt16 chr = eventP->data.keyDown.chr;

                /* Page Up / Page Down arrive with their ASCII control codes;
                 * the hardware up/down keys use different virtual codes on
                 * different ROMs, so accept both. */
                if (chr == vchrPageUp || chr == chrUpArrow) {
                    if (ReaderPrevPage()) {
                        ReaderDrawAll();
                    }
                    handled = true;
                } else if (chr == vchrPageDown || chr == chrDownArrow) {
                    if (ReaderNextPage()) {
                        ReaderDrawAll();
                    }
                    handled = true;
                }
            }
            break;

        default:
            break;
    }

    return handled;
}
