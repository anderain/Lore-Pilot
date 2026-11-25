/*
 * ReaderForm.c
 *
 * Full-screen page reader for a single LPBK volume. Layout mirrors
 * lore-teller/src/lib/render.ts.
 *
 *   y 0..7      black band, white 6x8 ASCII: displayName + "N/T"
 *   y 8..149    white body, glyph font, vertically centred
 *   y 150..157  black band, built-in 8x8 Chinese footer labels
 *   y 158       white progress border
 *   y 159       white progress fill (160 * page / total)
 *
 * Files written by older lore-teller builds have progressRows == 0: the body
 * then runs 8..151 and the footer sits at 152..159, with no progress bar.
 *
 * A page is composed in a screen buffer and flushed with one WinDrawBitmap,
 * so page turns never flash: the old "erase the whole screen, then draw into
 * it" sequence is gone. If the 3.2 KB buffer cannot be allocated the drawing
 * calls fall back to WinDrawLine and the reader still works, one glyph run at
 * a time.
 *
 * Volumes are standalone: turning past the last page or before the first page
 * does nothing, so a 1 MB device only ever needs the volume being read.
 *
 * Only Palm OS 1.0/2.x calls are used.
 */

#include <PalmOS.h>

#include "ReaderForm.h"
#include "TextDb.h"
#include "GlyphFont.h"
#include "ScreenBuf.h"
#include "UiFont.h"
#include "UiGlyphs.h"
#include "GotoForm.h"
#include "LorePilotReader_Rsc.h"

#define kReaderW            160
#define kReaderH            160
#define kReaderDefaultHdr   8
#define kReaderDefaultFtr   8
#define kReaderProgressRows 2

/* Bottom tap strip, split into four equal columns of 40px: previous page,
 * jump to page, back to the bookshelf, next page. It starts at y 148, below
 * the last possible body glyph row in both layouts (8px text ends at y 146),
 * so no text is ever covered. */
#define kReaderTapTop       148
#define kReaderTapZoneW     (kReaderW / 4)          /* 40 */
#define kReaderTapZone1     kReaderTapZoneW         /* 40  end of "prev"   */
#define kReaderTapZone2     (2 * kReaderTapZoneW)   /* 80  end of "goto"   */
#define kReaderTapZone3     (3 * kReaderTapZoneW)   /* 120 end of "shelf"  */

static TextDb     gReaderDb;
static Boolean    gReaderOpen = false;
static TextDbPage gReaderPage;
static GlyphFont  gReaderFont;
static Boolean    gReaderFontReady = false;

static UInt16     gReaderCell = 8;
static UInt16     gReaderPageIndex = 0;
static Boolean    gReaderRequested = false;
/* Set while the reader hands the screen to the goto panel, so the
 * frmCloseEvent that FrmGotoForm delivers does not close the book. */
static Boolean    gGotoPending = false;

/* Footer labels, drawn from the built-in glyphs so they work without any
 * installed font: prev page / goto / back to shelf / next page.
 *
 * Each label is centred inside its own 40px column. */
static const UInt16 kReaderLabelPrev[]  = { 0xC9CF, 0xD2BB, 0xD2B3 };
static const UInt16 kReaderLabelGoto[]  = { 0xCCF8, 0xD7AA };
static const UInt16 kReaderLabelShelf[] = { 0xBBD8, 0xCAE9, 0xBCDC };
static const UInt16 kReaderLabelNext[]  = { 0xCFC2, 0xD2BB, 0xD2B3 };

#define kReaderLabelX(zone, count) \
    ((Int16)((zone) * kReaderTapZoneW + \
             (kReaderTapZoneW - (count) * kUiGlyphWidth) / 2))

#define kReaderPrevX   kReaderLabelX(0, 3)
#define kReaderGotoX   kReaderLabelX(1, 2)
#define kReaderShelfX  kReaderLabelX(2, 3)
#define kReaderNextX   kReaderLabelX(3, 3)

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
                               (Int16)y, rows);
        }
    }
}

static void ReaderDrawHeader(void)
{
    Char label[40];
    UInt16 len;
    Int16  x;

    ScreenBufFillBand(0, ReaderBodyTop());

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
    ScreenBufFillBand(top, (UInt16)(rows + progressRows));

    UiGlyphsDrawText(kReaderPrevX, (Int16)top, kReaderLabelPrev,
                     (UInt16)(sizeof(kReaderLabelPrev) / sizeof(UInt16)), true);
    UiGlyphsDrawText(kReaderGotoX, (Int16)top, kReaderLabelGoto,
                     (UInt16)(sizeof(kReaderLabelGoto) / sizeof(UInt16)), true);
    UiGlyphsDrawText(kReaderShelfX, (Int16)top, kReaderLabelShelf,
                     (UInt16)(sizeof(kReaderLabelShelf) / sizeof(UInt16)), true);
    UiGlyphsDrawText(kReaderNextX, (Int16)top, kReaderLabelNext,
                     (UInt16)(sizeof(kReaderLabelNext) / sizeof(UInt16)), true);

    if (progressRows < kReaderProgressRows) {
        return;
    }

    borderY = (UInt16)(top + rows);
    fillY = (UInt16)(borderY + 1);

    /* Border: full-width white line. */
    ScreenBufHLine((Int16)borderY, 0, kReaderW - 1, kScreenInkWhite);

    /* Fill: white from x=0, length = round(160 * page / total). */
    total = gReaderDb.header.totalPages;
    if (total == 0) total = 1;
    page = ReaderGlobalPage();
    if (page > total) page = total;

    filled = (UInt32)kReaderW * page;
    filled = (filled + total / 2) / total;
    if (filled > (UInt32)kReaderW) filled = (UInt32)kReaderW;

    if (filled > 0 && fillY < kReaderH) {
        ScreenBufHLine((Int16)fillY, 0, (Int16)(filled - 1), kScreenInkWhite);
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

    if (bottom <= top) return;
    height = (UInt16)(bottom - top);

    ScreenBufClearRect(top, height);

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

/* Compose the whole page in the frame buffer, then show it in one go. */
static void ReaderPaint(void)
{
    ScreenBufClearRect(0, kReaderH);

    if (!gReaderOpen) {
        ScreenBufFlush();
        return;
    }

    ReaderDrawBody();
    ReaderDrawHeader();
    ReaderDrawFooter();
    ScreenBufFlush();
}

/*------------------------------------------------------------------ loading*/

static void ReaderFormClose(void);

static void ReaderOpenFont(void)
{
    if (gReaderFontReady) {
        GlyphFontClose(&gReaderFont);
        gReaderFontReady = false;
    }
    MemSet(&gReaderFont, sizeof(gReaderFont), 0);
    gReaderFontReady = GlyphFontFind(&gReaderFont, (UInt8)gReaderCell);
}

static Boolean ReaderLoadPage(UInt16 index)
{
    if (!gReaderOpen) return false;
    if ((UInt32)index >= gReaderDb.header.pageCount) return false;

    gReaderPageIndex = index;
    if (!TextDbReadPage(&gReaderDb, index, &gReaderPage)) {
        MemSet(&gReaderPage, sizeof(gReaderPage), 0);
    }
    return true;
}

/* Page turning stops at the volume boundaries: a volume is standalone. */
static Boolean ReaderNextPage(void)
{
    if (!gReaderOpen) return false;
    if ((UInt32)(gReaderPageIndex + 1) >= gReaderDb.header.pageCount) {
        return false;
    }
    return ReaderLoadPage((UInt16)(gReaderPageIndex + 1));
}

static Boolean ReaderPrevPage(void)
{
    if (!gReaderOpen) return false;
    if (gReaderPageIndex == 0) return false;
    return ReaderLoadPage((UInt16)(gReaderPageIndex - 1));
}

/*------------------------------------------------------------------ public*/

Boolean ReaderFormRequest(const Char *dbName, UInt16 cell)
{
    TextDb next;

    gReaderRequested = false;

    if (dbName == 0 || dbName[0] == '\0') return false;

    if (!TextDbOpen(&next, dbName)) {
        ReaderFormClose();
        return false;
    }

    if (gReaderOpen) {
        TextDbClose(&gReaderDb);
    }
    gReaderDb = next;
    gReaderOpen = true;
    gReaderPageIndex = 0;
    if (!TextDbReadPage(&gReaderDb, 0, &gReaderPage)) {
        MemSet(&gReaderPage, sizeof(gReaderPage), 0);
    }

    /* Trust the cell size recorded in the database header; `cell` is only a
     * hint so the font opens even if the header is odd. */
    gReaderCell = gReaderDb.header.cell;
    if (gReaderCell != 8 && gReaderCell != 12) {
        gReaderCell = (UInt16)((cell == 12) ? 12 : 8);
    }
    ReaderOpenFont();

    /* The reader owns the whole screen, so the buffer covers all 160 rows.
     * Failing to get it is survivable: drawing falls back to the screen. */
    (void)ScreenBufAlloc(kReaderH);

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
    ScreenBufFree();
    gReaderRequested = false;
}

/* Bottom four-tap strip: previous / goto / bookshelf / next. */
static Boolean ReaderHandleTap(const EventType *eventP)
{
    Int16 x;
    Int16 y;

    if (eventP->eType == penUpEvent) {
        x = eventP->data.penUp.end.x;
        y = eventP->data.penUp.end.y;
    } else {
        x = eventP->screenX;
        y = eventP->screenY;
    }

    if (y < kReaderTapTop) return false;

    /* Swallow the pen-down so nothing else reacts; the action happens on the
     * pen-up, which also keeps a modal dialog from opening mid-stroke. */
    if (eventP->eType == penDownEvent) return true;

    if (x < kReaderTapZone1) {
        if (ReaderPrevPage()) {
            ReaderPaint();
        }
        return true;
    }

    if (x < kReaderTapZone2) {
        /* Hand off to the self-drawn keypad panel; it returns to this form.
         * Release the frame buffer first: the keypad draws with the direct
         * path, and a live buffer would swallow its output. */
        GotoFormRequest(gReaderDb.header.firstPage,
                        gReaderDb.header.firstPage +
                            gReaderDb.header.pageCount - 1,
                        ReaderGlobalPage());
        gGotoPending = true;
        ScreenBufFree();
        FrmGotoForm(GotoForm);
        return true;
    }

    if (x < kReaderTapZone3) {
        /* Close the database before switching forms, like the test forms do:
         * frmCloseEvent delivery order is not guaranteed across ROMs. */
        ReaderFormClose();
        FrmGotoForm(MainForm);
        return true;
    }

    if (ReaderNextPage()) {
        ReaderPaint();
    }
    return true;
}

Boolean ReaderFormHandleEvent(EventType *eventP)
{
    Boolean handled = false;

    switch (eventP->eType) {
        case frmOpenEvent:
            FrmDrawForm(FrmGetActiveForm());
            /* Any pending goto hand-off is over once this form is back. */
            gGotoPending = false;
            if (gReaderRequested && gReaderOpen) {
                /* Coming back from the goto panel? Apply its result. */
                UInt32 page;

                /* The buffer was released when the keypad took over. */
                (void)ScreenBufAlloc(kReaderH);
                if (GotoFormTakePage(&page)) {
                    (void)ReaderLoadPage(
                        (UInt16)(page - gReaderDb.header.firstPage));
                }
                ReaderPaint();
            }
            handled = true;
            break;

        case frmUpdateEvent:
            if (gReaderOpen) {
                /* FrmDrawForm has already wiped the screen: rebuild the frame
                 * buffer and put it back, instead of the window server's
                 * partial repaint, which would flash. */
                ReaderPaint();
            }
            handled = true;
            break;

        case frmCloseEvent:
            if (gGotoPending) {
                /* Handing the screen to the goto panel: keep the book open. */
                gGotoPending = false;
            } else {
                ReaderFormClose();
            }
            handled = true;
            break;

        case penDownEvent:
        case penUpEvent:
            handled = ReaderHandleTap(eventP);
            break;

        case keyDownEvent:
            {
                UInt16 chr = eventP->data.keyDown.chr;

                /* Page Up / Page Down arrive with their ASCII control codes;
                 * the hardware up/down keys use different virtual codes on
                 * different ROMs, so accept both. */
                if (chr == vchrPageUp || chr == chrUpArrow) {
                    if (ReaderPrevPage()) {
                        ReaderPaint();
                    }
                    handled = true;
                } else if (chr == vchrPageDown || chr == chrDownArrow) {
                    if (ReaderNextPage()) {
                        ReaderPaint();
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
