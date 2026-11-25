/*
 * GlyphFont.c
 *
 * Paged GB2312 glyph font for LorePilotReader.
 *
 * PDB layout (version 2, layout 1):
 *   record 0: 48-byte header
 *   record 1..N: page, pageGlyphs * bytesPerGlyph bytes each
 *   index = (hi - firstRow) * cols + (lo - firstCol)
 *   page  = index / pageGlyphs, offset = (index % pageGlyphs) * bytesPerGlyph
 *
 * Supported cells: 8x8 (8 bytes/glyph, 2048 per page) and
 * 12x12 (24 bytes/glyph, 512 per page). Everything else is rejected.
 *
 * Only Dm*, Mem* and WinDrawBitmap (through ScreenBuf) are used, so this runs
 * on Palm OS 2.0.
 */

#include <PalmOS.h>

#include "GlyphFont.h"
#include "ScreenBuf.h"

#define kGlyphFontHeaderLen  48
#define kGlyphFontMaxPages   32

static UInt16 GlyphFontBe16(const UInt8 *p)
{
    return (UInt16)(((UInt16)p[0] << 8) | (UInt16)p[1]);
}

static UInt32 GlyphFontBe32(const UInt8 *p)
{
    return ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) |
           ((UInt32)p[2] << 8)  |  (UInt32)p[3];
}

static void GlyphFontResetCache(GlyphFont *font)
{
    UInt16 i;

    for (i = 0; i < kGlyphFontCachePages; i++) {
        font->cachedPage[i] = 0xFFFF;
        font->cachedHandle[i] = 0;
        font->cachedPtr[i] = 0;
    }
    font->nextVictim = 0;
}

static void GlyphFontDropCache(GlyphFont *font)
{
    UInt16 i;

    for (i = 0; i < kGlyphFontCachePages; i++) {
        if (font->cachedHandle[i] != 0) {
            if (font->cachedPtr[i] != 0) {
                MemHandleUnlock(font->cachedHandle[i]);
            }
            font->cachedHandle[i] = 0;
            font->cachedPtr[i] = 0;
        }
        font->cachedPage[i] = 0xFFFF;
    }
}

static void GlyphFontSetName(GlyphFont *font, const Char *name)
{
    UInt16 i;

    for (i = 0; i < 31 && name[i] != '\0'; i++) {
        font->name[i] = name[i];
    }
    font->name[i] = '\0';
}

/* Read and validate the 48-byte header of an open database. */
static Boolean GlyphFontReadHeader(const UInt8 *hdr, UInt16 recCount,
                                   GlyphFontInfo *info)
{
    UInt16 cellW, cellH, bytesPerGlyph, pageGlyphs, pages, extraRecord;

    if (hdr[0] != 'L' || hdr[1] != 'P' || hdr[2] != 'G' || hdr[3] != '1') return false;
    if (GlyphFontBe16(hdr + 4) != 2) return false;          /* version 2 */
    if (GlyphFontBe16(hdr + 14) != 1) return false;         /* paged layout */

    cellW        = GlyphFontBe16(hdr + 6);
    cellH        = GlyphFontBe16(hdr + 8);
    bytesPerGlyph = GlyphFontBe16(hdr + 10);
    pages        = GlyphFontBe16(hdr + 44);
    pageGlyphs   = GlyphFontBe16(hdr + 46);
    extraRecord  = (UInt16)GlyphFontBe32(hdr + 28);

    if (cellW != cellH) return false;
    if (cellW != 8 && cellW != 12) return false;
    if (bytesPerGlyph != cellH * ((cellW + 7) / 8)) return false;
    if (pages < 1 || pages > kGlyphFontMaxPages || pageGlyphs < 1) return false;
    if (GlyphFontBe16(hdr + 40) < 1 || GlyphFontBe16(hdr + 42) < 1) return false;

    if (extraRecord != 0) {
        if (extraRecord != (UInt16)(pages + 1)) return false;
        if (recCount != (UInt16)(pages + 2)) return false;
    } else if (recCount != (UInt16)(pages + 1)) {
        return false;
    }

    if (info != 0) {
        info->cellW = cellW;
        info->cellH = cellH;
        info->glyphCount = GlyphFontBe16(hdr + 12);
        info->pageCount = pages;
        info->valid = true;
    }
    return true;
}

/* Read only the header record of a database, without keeping it open. */
Boolean GlyphFontProbe(const Char *name, GlyphFontInfo *info)
{
    UInt8 hdr[kGlyphFontHeaderLen];
    LocalID dbID;
    DmOpenRef db;
    MemHandle h;
    const UInt8 *p;
    UInt16 recCount;
    UInt32 recordCount;
    UInt32 totalBytes;
    UInt32 dataBytes;

    if (info == 0) return false;
    MemSet(info, sizeof(*info), 0);

    dbID = DmFindDatabase(0, name);
    if (dbID == 0) return false;

    {
        UInt16 i;
        for (i = 0; i < 31 && name[i] != '\0'; i++) {
            info->name[i] = name[i];
        }
        info->name[i] = '\0';
    }
    info->dbID = dbID;

    recordCount = 0;
    totalBytes = 0;
    dataBytes = 0;
    if (DmDatabaseSize(0, dbID, &recordCount, &totalBytes, &dataBytes) == errNone) {
        info->sizeBytes = totalBytes;
    }

    db = DmOpenDatabase(0, dbID, dmModeReadOnly);
    if (db == 0) return false;

    recCount = DmNumRecords(db);
    if (recCount < 2) {
        DmCloseDatabase(db);
        return false;
    }

    h = DmQueryRecord(db, 0);
    if (h == 0 || MemHandleSize(h) < kGlyphFontHeaderLen) {
        DmCloseDatabase(db);
        return false;
    }
    p = (const UInt8 *)MemHandleLock(h);
    if (p == 0) {
        DmCloseDatabase(db);
        return false;
    }
    MemMove(hdr, p, kGlyphFontHeaderLen);
    MemHandleUnlock(h);
    DmCloseDatabase(db);

    return GlyphFontReadHeader(hdr, recCount, info);
}

Boolean GlyphFontOpen(GlyphFont *font, const Char *name)
{
    UInt8 hdr[kGlyphFontHeaderLen];
    LocalID dbID;
    DmOpenRef db;
    MemHandle h;
    const UInt8 *p;
    UInt16 recCount;
    GlyphFontInfo info;

    if (font == 0) return false;
    MemSet(font, sizeof(*font), 0);
    GlyphFontResetCache(font);

    dbID = DmFindDatabase(0, name);
    if (dbID == 0) return false;

    db = DmOpenDatabase(0, dbID, dmModeReadOnly);
    if (db == 0) return false;

    recCount = DmNumRecords(db);
    if (recCount < 2) {
        DmCloseDatabase(db);
        return false;
    }

    h = DmQueryRecord(db, 0);
    if (h == 0 || MemHandleSize(h) < kGlyphFontHeaderLen) {
        DmCloseDatabase(db);
        return false;
    }
    p = (const UInt8 *)MemHandleLock(h);
    if (p == 0) {
        DmCloseDatabase(db);
        return false;
    }
    MemMove(hdr, p, kGlyphFontHeaderLen);
    MemHandleUnlock(h);

    if (!GlyphFontReadHeader(hdr, recCount, &info)) {
        DmCloseDatabase(db);
        return false;
    }

    font->version        = 2;
    font->cellW          = GlyphFontBe16(hdr + 6);
    font->cellH          = GlyphFontBe16(hdr + 8);
    font->bytesPerGlyph  = GlyphFontBe16(hdr + 10);
    font->glyphCount     = GlyphFontBe16(hdr + 12);
    font->firstRow       = GlyphFontBe16(hdr + 32);
    font->lastRow        = GlyphFontBe16(hdr + 34);
    font->firstCol       = GlyphFontBe16(hdr + 36);
    font->lastCol        = GlyphFontBe16(hdr + 38);
    font->rows           = GlyphFontBe16(hdr + 40);
    font->cols           = GlyphFontBe16(hdr + 42);
    font->pageCount      = GlyphFontBe16(hdr + 44);
    font->pageGlyphs     = GlyphFontBe16(hdr + 46);
    font->extraRecord    = (UInt16)GlyphFontBe32(hdr + 28);

    font->db = db;
    font->open = true;
    font->extraCount = 0;

    if (font->extraRecord != 0) {
        /* Load the small extras table once; it holds out-of-grid glyphs
           used by the bundled text. */
        MemHandle eh = DmQueryRecord(db, font->extraRecord);
        if (eh != 0) {
            const UInt8 *ep = (const UInt8 *)MemHandleLock(eh);
            if (ep != 0) {
                UInt16 count = GlyphFontBe16(ep);
                UInt16 itemSize = (UInt16)(2 + font->bytesPerGlyph);
                UInt16 i;
                if (count > kGlyphFontMaxExtras) count = kGlyphFontMaxExtras;
                if (MemHandleSize(eh) >= (UInt32)(2 + (UInt32)count * itemSize)) {
                    for (i = 0; i < count; i++) {
                        UInt16 k;
                        UInt16 base = (UInt16)(2 + i * itemSize);
                        font->extraCode[i] = GlyphFontBe16(ep + base);
                        for (k = 0; k < font->bytesPerGlyph; k++) {
                            font->extraBytes[i][k] = ep[base + 2 + k];
                        }
                    }
                    font->extraCount = count;
                }
                MemHandleUnlock(eh);
            }
        }
    }

    GlyphFontSetName(font, name);
    return true;
}

/* Lexicographic compare for DB names; keeps 08 before 12 in the font list. */
static Int16 GlyphFontCompareName(const Char *a, const Char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) return (Int16)((UInt8)*a - (UInt8)*b);
        a++;
        b++;
    }
    return (Int16)((UInt8)*a - (UInt8)*b);
}

/* Enumerate every GLYF/LPRD font database. */
UInt16 GlyphFontList(GlyphFontInfo *info, UInt16 maxEntries)
{
    UInt16 total;
    UInt16 i;
    UInt16 found = 0;

    if (info == 0 || maxEntries == 0) return 0;
    total = DmNumDatabases(0);

    for (i = 0; i < total && found < maxEntries; i++) {
        LocalID dbID = DmGetDatabase(0, i);
        Char name[32];
        UInt32 type = 0;
        UInt32 creator = 0;

        if (dbID == 0) continue;
        MemSet(name, sizeof(name), 0);
        if (DmDatabaseInfo(0, dbID, name, 0, 0, 0, 0, 0, 0, 0, 0,
                           &type, &creator) != errNone) {
            continue;
        }
        if (type != 'GLYF' || creator != 'LPRD') continue;
        if (!GlyphFontProbe(name, &info[found])) continue;
        found++;
    }

    /* Insertion sort by name: LPF08* before LPF12*, levels B < F < H. */
    for (i = 1; i < found; i++) {
        GlyphFontInfo tmp = info[i];
        Int16 j = (Int16)(i - 1);

        while (j >= 0 && GlyphFontCompareName(info[j].name, tmp.name) > 0) {
            info[j + 1] = info[j];
            j--;
        }
        info[j + 1] = tmp;
    }
    return found;
}

/* Pick the database with the most glyphs for a given cell size. */
Boolean GlyphFontFind(GlyphFont *font, UInt8 cell)
{
    GlyphFontInfo list[16];
    UInt16 count;
    UInt16 i;
    Int16 best = -1;

    count = GlyphFontList(list, 16);
    for (i = 0; i < count; i++) {
        if (list[i].cellW != cell) continue;
        if (best < 0 || list[i].glyphCount > list[best].glyphCount) {
            best = (Int16)i;
        }
    }
    if (best < 0) return false;
    return GlyphFontOpen(font, list[best].name);
}

Boolean GlyphFontOpenAny(GlyphFont *font)
{
    if (GlyphFontFind(font, 8)) return true;
    if (GlyphFontFind(font, 12)) return true;
    return false;
}

void GlyphFontClose(GlyphFont *font)
{
    if (font == 0) return;

    GlyphFontDropCache(font);
    if (font->db != 0) {
        DmCloseDatabase(font->db);
    }
    font->db = 0;
    font->open = false;
}

Boolean GlyphFontSlot(GlyphFont *font, UInt16 code, UInt16 *page, UInt16 *offset)
{
    UInt16 hi = (UInt16)((code >> 8) & 0xFF);
    UInt16 lo = (UInt16)(code & 0xFF);
    UInt32 index;

    if (!font || !font->open) return false;
    if (hi < font->firstRow || hi > font->lastRow ||
        lo < font->firstCol || lo > font->lastCol) {
        return false;
    }

    index = (UInt32)(hi - font->firstRow) * font->cols + (UInt32)(lo - font->firstCol);
    *page = (UInt16)(index / font->pageGlyphs);
    *offset = (UInt16)((index % font->pageGlyphs) * font->bytesPerGlyph);
    return *page < font->pageCount;
}

static UInt8* GlyphFontGetPage(GlyphFont *font, UInt16 page)
{
    UInt16 i;

    for (i = 0; i < kGlyphFontCachePages; i++) {
        if (font->cachedPage[i] == page && font->cachedPtr[i] != 0) {
            return font->cachedPtr[i];
        }
    }

    i = font->nextVictim;
    font->nextVictim = (UInt16)((font->nextVictim + 1) % kGlyphFontCachePages);

    if (font->cachedHandle[i] != 0) {
        if (font->cachedPtr[i] != 0) {
            MemHandleUnlock(font->cachedHandle[i]);
        }
        font->cachedHandle[i] = 0;
        font->cachedPtr[i] = 0;
        font->cachedPage[i] = 0xFFFF;
    }

    {
        MemHandle h = DmQueryRecord(font->db, (UInt16)(page + 1));
        const UInt8 *p;

        if (h == 0) return 0;
        if (MemHandleSize(h) < (UInt32)font->pageGlyphs * font->bytesPerGlyph) {
            return 0;
        }
        p = (const UInt8 *)MemHandleLock(h);
        if (p == 0) return 0;

        font->cachedHandle[i] = h;
        font->cachedPtr[i] = (UInt8 *)p;
        font->cachedPage[i] = page;
        return font->cachedPtr[i];
    }
}

Boolean GlyphFontGet(GlyphFont *font, UInt16 code, UInt8 *out)
{
    UInt16 page;
    UInt16 offset;
    UInt8 *base;
    UInt16 i;

    if (!font || !font->open) return false;

    /* Out-of-grid extras first: they are GBK-only characters or level-2
       hanzi when the basic database is installed. */
    if (!GlyphFontSlot(font, code, &page, &offset)) {
        for (i = 0; i < font->extraCount; i++) {
            if (font->extraCode[i] == code) {
                UInt16 k;
                for (k = 0; k < font->bytesPerGlyph; k++) {
                    out[k] = font->extraBytes[i][k];
                }
                return true;
            }
        }
        return false;
    }

    base = GlyphFontGetPage(font, page);
    if (base == 0) {
        return false;
    }
    for (i = 0; i < font->bytesPerGlyph; i++) {
        out[i] = base[offset + i];
    }
    return true;
}

void GlyphFontDrawGlyph(const GlyphFont *font, Int16 x, Int16 y,
                        const UInt8 *rows)
{
    if (font == 0 || rows == 0) return;

    ScreenBufCell(x, y, rows, font->cellW, kScreenInkBlack);
}
