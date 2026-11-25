/*
 * GlyphTest.c
 *
 * 8x8 glyph database test for LorePilotReader.
 *
 * Target: Palm OS 2.0 (Pilot Professional, 160x160, 1MB).
 *
 * Deliberately uses only Palm OS 1.0 APIs:
 *   Dm* record database calls, Mem* handle calls, Frm* forms,
 *   ScreenBuf (WinDrawBitmap / WinDrawLine) and WinDrawChars.
 *
 * Glyphs are composed into the frame buffer and flushed in one go; the ASCII
 * status lines are drawn with the system font after the flush. Everything is
 * 1x: the 2x copies only existed to exercise the scaled run drawing, which the
 * reader never used. The odd-offset and edge-clipping probes are kept, because
 * they are the only on-device exercise of the buffer's shift and clip paths.
 *
 * PDB layout (record 0 header):
 *   magic 'LPG1', version, cell size, glyph count, flags, checksum,
 *   and a text-record index in the old reserved field (offset 28).
 *   Records 1..N are glyphs; record textRecord (if non-zero) is the phrase:
 *     u16 version, u16 count, u16 glyphIndex[count], u16 codepoint[count]
 */

#include <PalmOS.h>

#include "GlyphTest.h"
#include "ScreenBuf.h"
#include "LorePilotReader_Rsc.h"
#include "GlyphAtlas.h"

#define kGlyphTestMaxGlyphs     32
#define kGlyphTestMaxPhrase     64
#define kGlyphTestFormW         160
#define kGlyphTestFormH         160

/* Body area above the form's buttons (they start at y 146). */
#define kGlyphTestBodyH         144

#define kGlyphTestDbName        "LPT08A"
#define kGlyphTestDbLegacyName  "LPGlyph8"

#define kGlyphTestFontHeaderLen 32

/* status codes */
#define kGlyphTestOk            0
#define kGlyphTestNoDb          1
#define kGlyphTestBadHdr        2
#define kGlyphTestBadRec        3
#define kGlyphTestMismatch      4
#define kGlyphTestBadCount      5

typedef struct GlyphTestStateType {
    UInt16  status;
    UInt16  version;
    UInt16  cellW;
    UInt16  cellH;
    UInt16  bytesPerGlyph;
    UInt16  glyphCount;
    UInt16  layout;
    UInt16  flags;
    UInt16  firstCode;
    UInt16  compression;
    UInt16  packing;
    UInt16  headerChecksum;
    UInt16  calcChecksum;
    UInt16  okCount;
    Int16   firstBad;
    Boolean fromDb;
    Boolean sumOk;
    Boolean phraseFromDb;
    Boolean phraseOk;
    UInt16  textRecord;
    UInt16  phraseCount;
    const Char *dbName;
    UInt16  phrase[kGlyphTestMaxPhrase];
    UInt8   rows[kGlyphTestMaxGlyphs][8];
} GlyphTestStateType;

static GlyphTestStateType gGlyphTest;
static UInt16              gGlyphSelected = 0;

/*------------------------------------------------------------------ bytes*/

static UInt16 GlyphTestGetBE16(const UInt8 *p)
{
    return (UInt16)(((UInt16)p[0] << 8) | (UInt16)p[1]);
}

static UInt32 GlyphTestGetBE32(const UInt8 *p)
{
    return ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) |
           ((UInt32)p[2] << 8)  |  (UInt32)p[3];
}

/* Palm OS 2.0: avoid depending on the 2.x MemCmp trap. */
static Boolean GlyphTestBytesEqual(const UInt8 *a, const UInt8 *b, UInt16 len)
{
    UInt16 i;

    for (i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static void GlyphTestAppendInt(Char *dst, UInt16 value)
{
    Char tmp[8];

    StrIToA(tmp, (Int32)value);
    StrCat(dst, tmp);
}

static void GlyphTestAppendStr(Char *dst, const Char *src)
{
    StrCat(dst, src);
}

static void GlyphTestAppendHexByte(Char *dst, UInt8 value)
{
    static const Char hex[] = "0123456789ABCDEF";
    Char tmp[3];

    tmp[0] = hex[(value >> 4) & 0x0F];
    tmp[1] = hex[value & 0x0F];
    tmp[2] = '\0';
    StrCat(dst, tmp);
}

/*------------------------------------------------------------------ model*/

static void GlyphTestLoadBuiltinPhrase(void)
{
    UInt16 i;
    UInt16 count;

    count = (UInt16)kGlyphAtlasPhraseCount;
    if (count > kGlyphTestMaxPhrase) {
        count = kGlyphTestMaxPhrase;
    }
    for (i = 0; i < count; i++) {
        gGlyphTest.phrase[i] = (UInt16)kGlyphAtlasPhrase[i];
    }
    gGlyphTest.phraseCount = count;
    gGlyphTest.phraseFromDb = false;
    gGlyphTest.phraseOk = false;
}

static void GlyphTestUseBuiltin(void)
{
    UInt16 i;

    for (i = 0; i < kGlyphAtlasCount && i < kGlyphTestMaxGlyphs; i++) {
        MemMove(gGlyphTest.rows[i],
                (const void *)(kGlyphAtlasBytes + (UInt32)i * kGlyphAtlasBytesPerGlyph),
                kGlyphAtlasBytesPerGlyph);
    }
    gGlyphTest.glyphCount = (UInt16)(kGlyphAtlasCount > kGlyphTestMaxGlyphs
                                     ? kGlyphTestMaxGlyphs
                                     : kGlyphAtlasCount);
    gGlyphTest.fromDb = false;
    gGlyphTest.phraseFromDb = false;
    if (gGlyphTest.phraseCount == 0) {
        GlyphTestLoadBuiltinPhrase();
    }
}

static Boolean GlyphTestReadRecord(DmOpenRef db, UInt16 index, UInt8 *dst, UInt16 len)
{
    MemHandle h;
    const UInt8 *p;

    h = DmQueryRecord(db, index);
    if (h == 0) {
        return false;
    }
    if (MemHandleSize(h) < (UInt32)len) {
        return false;
    }
    p = (const UInt8 *)MemHandleLock(h);
    if (p == 0) {
        return false;
    }
    MemMove(dst, p, len);
    MemHandleUnlock(h);
    return true;
}

static Boolean GlyphTestReadPhrase(DmOpenRef db, UInt16 index)
{
    UInt8 head[4];
    UInt8 buf[4 + kGlyphTestMaxPhrase * 4];
    UInt16 count;
    UInt16 i;

    if (!GlyphTestReadRecord(db, index, head, 4)) {
        return false;
    }
    if (GlyphTestGetBE16(head) != 1) {
        return false;
    }
    count = GlyphTestGetBE16(head + 2);
    if (count < 1 || count > kGlyphTestMaxPhrase) {
        return false;
    }
    if (!GlyphTestReadRecord(db, index, buf, (UInt16)(4 + count * 4))) {
        return false;
    }
    for (i = 0; i < count; i++) {
        gGlyphTest.phrase[i] = GlyphTestGetBE16(buf + 4 + i * 2);
    }
    gGlyphTest.phraseCount = count;
    gGlyphTest.phraseFromDb = true;
    return true;
}

static Boolean GlyphTestPhraseMatchesBuiltin(void)
{
    UInt16 i;

    if (gGlyphTest.phraseCount != (UInt16)kGlyphAtlasPhraseCount) {
        return false;
    }
    for (i = 0; i < gGlyphTest.phraseCount; i++) {
        if (gGlyphTest.phrase[i] != (UInt16)kGlyphAtlasPhrase[i]) {
            return false;
        }
    }
    return true;
}

static void GlyphTestLoad(void)
{
    UInt8 hdr[kGlyphTestFontHeaderLen];
    LocalID dbID;
    DmOpenRef db;
    UInt16 recCount;
    UInt16 i;
    UInt16 sum;
    UInt16 count;
    UInt16 expectedRecords;

    MemSet(&gGlyphTest, sizeof(gGlyphTest), 0);
    gGlyphTest.status = kGlyphTestNoDb;
    gGlyphTest.firstBad = -1;
    gGlyphTest.sumOk = false;
    gGlyphTest.dbName = kGlyphTestDbName;
    GlyphTestLoadBuiltinPhrase();

    dbID = DmFindDatabase(0, kGlyphTestDbName);
    if (dbID == 0) {
        /* Pre-rename builds used LPGlyph8; keep reading it if present. */
        dbID = DmFindDatabase(0, kGlyphTestDbLegacyName);
        if (dbID != 0) {
            gGlyphTest.dbName = kGlyphTestDbLegacyName;
        }
    }
    db = 0;

    if (dbID != 0) {
        db = DmOpenDatabase(0, dbID, dmModeReadOnly);
    }
    if (db == 0) {
        /* Do not fall back to any GLYF/LPRD database: those are paged fonts
           (layout 1) with a different header, not the atlas test database. */
        GlyphTestUseBuiltin();
        gGlyphTest.status = kGlyphTestNoDb;
        return;
    }

    recCount = DmNumRecords(db);
    if (recCount < 1 || !GlyphTestReadRecord(db, 0, hdr, kGlyphTestFontHeaderLen)) {
        DmCloseDatabase(db);
        GlyphTestUseBuiltin();
        gGlyphTest.status = kGlyphTestBadHdr;
        return;
    }

    if (hdr[0] != 'L' || hdr[1] != 'P' || hdr[2] != 'G' || hdr[3] != '1') {
        DmCloseDatabase(db);
        GlyphTestUseBuiltin();
        gGlyphTest.status = kGlyphTestBadHdr;
        return;
    }

    gGlyphTest.version        = GlyphTestGetBE16(hdr + 4);
    gGlyphTest.cellW          = GlyphTestGetBE16(hdr + 6);
    gGlyphTest.cellH          = GlyphTestGetBE16(hdr + 8);
    gGlyphTest.bytesPerGlyph  = GlyphTestGetBE16(hdr + 10);
    gGlyphTest.glyphCount     = GlyphTestGetBE16(hdr + 12);
    gGlyphTest.layout         = GlyphTestGetBE16(hdr + 14);
    gGlyphTest.flags          = GlyphTestGetBE16(hdr + 18);
    gGlyphTest.firstCode      = GlyphTestGetBE16(hdr + 20);
    gGlyphTest.compression    = GlyphTestGetBE16(hdr + 22);
    gGlyphTest.headerChecksum = GlyphTestGetBE16(hdr + 24);
    gGlyphTest.packing        = GlyphTestGetBE16(hdr + 26);
    gGlyphTest.textRecord     = (UInt16)GlyphTestGetBE32(hdr + 28);

    if (gGlyphTest.version != 1 ||
        gGlyphTest.cellW != 8 || gGlyphTest.cellH != 8 ||
        gGlyphTest.bytesPerGlyph != 8 ||
        gGlyphTest.layout != 0 ||
        gGlyphTest.glyphCount < 1 ||
        gGlyphTest.glyphCount > kGlyphTestMaxGlyphs) {
        DmCloseDatabase(db);
        GlyphTestUseBuiltin();
        gGlyphTest.status = kGlyphTestBadHdr;
        return;
    }

    expectedRecords = (UInt16)(gGlyphTest.glyphCount + 1 +
                               (gGlyphTest.textRecord != 0 ? 1 : 0));
    if (gGlyphTest.glyphCount != (UInt16)kGlyphAtlasCount ||
        recCount != expectedRecords ||
        (gGlyphTest.textRecord != 0 &&
         (gGlyphTest.textRecord != (UInt16)kGlyphAtlasTextRecord ||
          gGlyphTest.textRecord >= recCount))) {
        DmCloseDatabase(db);
        GlyphTestUseBuiltin();
        gGlyphTest.status = kGlyphTestBadCount;
        return;
    }

    count = gGlyphTest.glyphCount;
    sum = 0;
    for (i = 0; i < count; i++) {
        if (!GlyphTestReadRecord(db, (UInt16)(i + 1), gGlyphTest.rows[i], 8)) {
            gGlyphTest.firstBad = (Int16)i;
            gGlyphTest.status = kGlyphTestBadRec;
            break;
        }
        sum = (UInt16)((sum + gGlyphTest.rows[i][0] + gGlyphTest.rows[i][1] +
                        gGlyphTest.rows[i][2] + gGlyphTest.rows[i][3] +
                        gGlyphTest.rows[i][4] + gGlyphTest.rows[i][5] +
                        gGlyphTest.rows[i][6] + gGlyphTest.rows[i][7]) & 0xFFFF);
    }

    if (gGlyphTest.status == kGlyphTestBadRec) {
        DmCloseDatabase(db);
        GlyphTestUseBuiltin();
        return;
    }

    if (gGlyphTest.textRecord != 0) {
        if (!GlyphTestReadPhrase(db, gGlyphTest.textRecord)) {
            DmCloseDatabase(db);
            GlyphTestUseBuiltin();
            gGlyphTest.status = kGlyphTestBadRec;
            return;
        }
    }

    DmCloseDatabase(db);

    gGlyphTest.fromDb = true;
    gGlyphTest.calcChecksum = sum;
    gGlyphTest.sumOk = ((gGlyphTest.flags & 0x0002) == 0) ||
                       (sum == gGlyphTest.headerChecksum);

    /* byte-for-byte comparison against the packed-in answer key */
    gGlyphTest.okCount = 0;
    gGlyphTest.firstBad = -1;
    for (i = 0; i < count; i++) {
        if (GlyphTestBytesEqual(gGlyphTest.rows[i],
                                (const UInt8 *)(kGlyphAtlasBytes + (UInt32)i * kGlyphAtlasBytesPerGlyph),
                                8)) {
            gGlyphTest.okCount++;
        } else if (gGlyphTest.firstBad < 0) {
            gGlyphTest.firstBad = (Int16)i;
        }
    }

    gGlyphTest.phraseOk = (gGlyphTest.phraseCount > 0) &&
                          GlyphTestPhraseMatchesBuiltin();

    if (gGlyphTest.okCount == count) {
        gGlyphTest.status = kGlyphTestOk;
    } else {
        gGlyphTest.status = kGlyphTestMismatch;
    }
}

/*------------------------------------------------------------------ drawing*/

static void GlyphTestClearBody(void)
{
    (void)ScreenBufAlloc(kGlyphTestBodyH);
    ScreenBufClearRect(0, kGlyphTestBodyH);
}

static void GlyphTestDrawText(Int16 x, Int16 y, const Char *s)
{
    WinDrawChars(s, StrLen(s), x, y);
}

/* One 8x8 glyph into the frame buffer. ScreenBuf clips negative x and the
 * right edge, which is what the probes below rely on. */
static void GlyphTestDrawGlyph(Int16 x, Int16 y, const UInt8 *rows)
{
    ScreenBufCell(x, y, rows, 8, kScreenInkBlack);
}

static void GlyphTestDrawPhrase(Int16 x, Int16 y, UInt16 first, UInt16 count)
{
    UInt16 i;
    UInt16 index;

    for (i = 0; i < count && (UInt16)(first + i) < gGlyphTest.phraseCount; i++) {
        index = gGlyphTest.phrase[first + i];
        if (index < gGlyphTest.glyphCount) {
            GlyphTestDrawGlyph((Int16)(x + (Int16)i * 8), y,
                               gGlyphTest.rows[index]);
        }
    }
}

static UInt16 GlyphTestCount(void)
{
    UInt16 count = gGlyphTest.glyphCount;

    if (count == 0) {
        count = (UInt16)(kGlyphAtlasCount > kGlyphTestMaxGlyphs
                         ? kGlyphTestMaxGlyphs : kGlyphAtlasCount);
    }
    return count;
}

static void GlyphTestDrawStatus(void)
{
    Char line[80];
    UInt16 count = GlyphTestCount();

    /* line 1: database name, version, glyph count, phrase length */
    line[0] = '\0';
    GlyphTestAppendStr(line, gGlyphTest.dbName);
    if (gGlyphTest.fromDb) {
        GlyphTestAppendStr(line, " v");
        GlyphTestAppendInt(line, gGlyphTest.version);
    } else {
        GlyphTestAppendStr(line, " builtin");
    }
    GlyphTestAppendStr(line, " ");
    GlyphTestAppendInt(line, count);
    GlyphTestAppendStr(line, "g P");
    GlyphTestAppendInt(line, gGlyphTest.phraseCount);
    GlyphTestDrawText(2, 13, line);

    /* line 2: result, kept short enough for 160px at 6px/char */
    line[0] = '\0';
    switch (gGlyphTest.status) {
        case kGlyphTestOk:
            GlyphTestAppendStr(line, "OK ");
            GlyphTestAppendInt(line, gGlyphTest.okCount);
            GlyphTestAppendStr(line, "/");
            GlyphTestAppendInt(line, count);
            GlyphTestAppendStr(line, gGlyphTest.sumOk ? " SUM OK" : " SUM NG");
            break;

        case kGlyphTestNoDb:
            GlyphTestAppendStr(line, "NO DB builtin");
            break;

        case kGlyphTestBadHdr:
            GlyphTestAppendStr(line, "BAD HDR builtin");
            break;

        case kGlyphTestBadRec:
            GlyphTestAppendStr(line, "BAD REC @");
            GlyphTestAppendInt(line, (UInt16)(gGlyphTest.firstBad < 0 ? 0 : gGlyphTest.firstBad));
            break;

        case kGlyphTestBadCount:
            GlyphTestAppendStr(line, "BAD CNT builtin");
            break;

        case kGlyphTestMismatch:
        default:
            GlyphTestAppendStr(line, "NG @");
            GlyphTestAppendInt(line, (UInt16)(gGlyphTest.firstBad < 0 ? 0 : gGlyphTest.firstBad));
            GlyphTestAppendStr(line, " ");
            GlyphTestAppendInt(line, gGlyphTest.okCount);
            GlyphTestAppendStr(line, "/");
            GlyphTestAppendInt(line, count);
            break;
    }
    if (gGlyphTest.phraseFromDb) {
        GlyphTestAppendStr(line, " P");
        GlyphTestAppendInt(line, gGlyphTest.phraseCount);
        GlyphTestAppendStr(line, gGlyphTest.phraseOk ? "OK" : "NG");
    }
    GlyphTestDrawText(2, 22, line);
}

static void GlyphTestDrawInfo(void)
{
    Char line[80];
    UInt16 i;
    UInt16 count = GlyphTestCount();
    const UInt8 *rows;

    if (gGlyphSelected >= count) {
        gGlyphSelected = 0;
    }
    rows = gGlyphTest.rows[gGlyphSelected];

    line[0] = '\0';
    GlyphTestAppendStr(line, "#");
    GlyphTestAppendInt(line, gGlyphSelected);
    GlyphTestAppendStr(line, " ");
    for (i = 0; i < 8; i++) {
        GlyphTestAppendHexByte(line, rows[i]);
    }
    GlyphTestDrawText(2, 136, line);
}

static void GlyphTestDrawAll(void)
{
    UInt16 i;
    UInt16 count;
    UInt16 phraseCount;
    Int16 x;

    GlyphTestClearBody();

    count = GlyphTestCount();

    /* All glyphs, two rows of up to 11, at 1x. */
    for (i = 0; i < count && i < 11; i++) {
        GlyphTestDrawGlyph((Int16)(2 + i * 9), 33, gGlyphTest.rows[i]);
    }
    for (i = 11; i < count && i < 22; i++) {
        GlyphTestDrawGlyph((Int16)(2 + (i - 11) * 9), 42, gGlyphTest.rows[i]);
    }

    phraseCount = gGlyphTest.phraseCount;

    /* Phrase at 1x, centred; over 20 units wraps onto a second row. */
    if (phraseCount > 0) {
        UInt16 first = phraseCount > 20 ? 20 : phraseCount;

        x = (Int16)((kGlyphTestFormW - (Int16)first * 8) / 2);
        if (x < 0) {
            x = 0;
        }
        GlyphTestDrawPhrase(x, 54, 0, first);

        if (phraseCount > 20) {
            UInt16 remain = (UInt16)(phraseCount - first);
            if (remain > 20) {
                remain = 20;
            }
            x = (Int16)((kGlyphTestFormW - (Int16)remain * 8) / 2);
            GlyphTestDrawPhrase(x, 64, first, remain);
        }
    }

    /* Odd x offsets: the buffer is byte addressed, so these land mid-byte
     * and catch a wrong shift mask. */
    for (i = 0; i < count && i < 4; i++) {
        GlyphTestDrawGlyph((Int16)(3 + i * 30), 78, gGlyphTest.rows[i]);
    }
    for (i = 0; i < count && i < 4; i++) {
        GlyphTestDrawGlyph((Int16)(11 + i * 30), 90, gGlyphTest.rows[i]);
    }

    /* Left and right clipping: partially off the buffer on each side. */
    if (count > 0) {
        GlyphTestDrawGlyph(-3, 104, gGlyphTest.rows[0]);
    }
    if (count > 1) {
        GlyphTestDrawGlyph(157, 104, gGlyphTest.rows[1]);
    }
    if (count > 2) {
        GlyphTestDrawGlyph(-8, 116, gGlyphTest.rows[2]);
    }
    if (count > 3) {
        GlyphTestDrawGlyph(160, 116, gGlyphTest.rows[3]);
    }

    /* Whole frame composed: one flush, then the ASCII status on top. */
    ScreenBufFlush();

    FntSetFont(stdFont);
    GlyphTestDrawText(2, 2, "Glyph DB Test");
    GlyphTestDrawStatus();
    GlyphTestDrawInfo();
}

/*------------------------------------------------------------------ events*/

Boolean GlyphTestFormHandleEvent(EventType *eventP)
{
    Boolean handled = false;

    switch (eventP->eType) {
        case frmOpenEvent:
            FrmDrawForm(FrmGetActiveForm());
            GlyphTestLoad();
            GlyphTestDrawAll();
            handled = true;
            break;

        case frmUpdateEvent:
            FrmDrawForm(FrmGetActiveForm());
            GlyphTestDrawAll();
            handled = true;
            break;

        case frmCloseEvent:
            ScreenBufFree();
            handled = true;
            break;

        case ctlSelectEvent:
            switch (eventP->data.ctlSelect.controlID) {
                case GlyphTestReloadButton:
                    GlyphTestLoad();
                    GlyphTestDrawAll();
                    handled = true;
                    break;

                case GlyphTestBackButton:
                    ScreenBufFree();
                    FrmGotoForm(MainForm);
                    handled = true;
                    break;

                default:
                    break;
            }
            break;

        case penDownEvent:
            /* tap in the two 1x rows selects a glyph for the hex line */
            if (eventP->screenY >= 31 && eventP->screenY < 52 &&
                eventP->screenX >= 2) {
                UInt16 rowOffset = (eventP->screenY >= 42) ? 11 : 0;
                UInt16 index = (UInt16)(rowOffset + (eventP->screenX - 2) / 9);
                UInt16 count = GlyphTestCount();

                if (index < count) {
                    gGlyphSelected = index;
                    GlyphTestDrawAll();
                    handled = true;
                }
            }
            break;

        default:
            break;
    }

    return handled;
}
