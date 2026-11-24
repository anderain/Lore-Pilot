/*
 * GlyphFont.c
 *
 * Paged GB2312 glyph font for LorePilotReader.
 *
 * PDB layout (version 2, layout 1):
 *   record 0: 48-byte header
 *   record 1..N: page, pageGlyphs * 8 bytes each
 *   index = (hi - firstRow) * cols + (lo - firstCol)
 *   page  = index / pageGlyphs, offset = (index % pageGlyphs) * 8
 *
 * Only Dm*, Mem* and WinDrawLine are used, so this runs on Palm OS 2.0.
 * Two page handles are cached; pages are never copied wholesale.
 */

#include <PalmOS.h>

#include "GlyphFont.h"

#define kGlyphFontHeaderLen  48
#define kGlyphFontMaxPages   16

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

Boolean GlyphFontOpen(GlyphFont *font, const Char *name)
{
    UInt8 hdr[kGlyphFontHeaderLen];
    LocalID dbID;
    DmOpenRef db;
    MemHandle h;
    const UInt8 *p;
    UInt16 recCount;

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

    if (hdr[0] != 'L' || hdr[1] != 'P' || hdr[2] != 'G' || hdr[3] != '1') {
        DmCloseDatabase(db);
        return false;
    }

    font->version        = GlyphFontBe16(hdr + 4);
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

    if (font->version != 2 ||
        font->cellW != 8 || font->cellH != 8 ||
        font->bytesPerGlyph != 8 ||
        GlyphFontBe16(hdr + 14) != 1) {     /* layout: paged */
        DmCloseDatabase(db);
        return false;
    }

    if (font->pageCount < 1 || font->pageCount > kGlyphFontMaxPages ||
        font->pageGlyphs < 1 ||
        font->rows < 1 || font->rows > 94 ||
        font->cols < 1 || font->cols > 94 ||
        font->firstCol > font->lastCol || font->firstRow > font->lastRow) {
        DmCloseDatabase(db);
        return false;
    }

    /* Expected records: header + pages, plus the optional extras record. */
    if (font->extraRecord != 0) {
        if (font->extraRecord != (UInt16)(font->pageCount + 1) ||
            recCount != (UInt16)(font->pageCount + 2)) {
            DmCloseDatabase(db);
            return false;
        }
    } else if (recCount != (UInt16)(font->pageCount + 1)) {
        DmCloseDatabase(db);
        return false;
    }

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
                UInt16 i;
                if (count > 128) count = 128;
                if (MemHandleSize(eh) >= (UInt32)(2 + count * 10)) {
                    for (i = 0; i < count; i++) {
                        UInt16 k;
                        font->extraCode[i] = GlyphFontBe16(ep + 2 + i * 10);
                        for (k = 0; k < 8; k++) {
                            font->extraBytes[i][k] = ep[2 + i * 10 + 2 + k];
                        }
                    }
                    font->extraCount = count;
                }
                MemHandleUnlock(eh);
            }
        }
    }
    {
        UInt16 i;
        for (i = 0; i < 31 && name[i] != '\0'; i++) {
            font->name[i] = name[i];
        }
        font->name[i] = '\0';
    }
    return true;
}

/* Local helper is not allowed on 2.0 as a method; kept as a static function. */
Boolean GlyphFontOpenAny(GlyphFont *font)
{
    static const Char *const names[] = {
        "LPGB2312F",
        "LPGB2312B",
        "LPGB2312"
    };
    UInt16 i;
    DmOpenRef db;

    for (i = 0; i < 3; i++) {
        if (GlyphFontOpen(font, names[i])) {
            return true;
        }
    }

    /* Last resort: any GLYF/LPRD database with the paged layout. */
    db = DmOpenDatabaseByTypeCreator('GLYF', 'LPRD', dmModeReadOnly);
    if (db != 0) {
        LocalID dbID;
        Char name[32];

        DmOpenDatabaseInfo(db, &dbID, 0, 0, 0, 0);
        DmDatabaseInfo(0, dbID, name, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        DmCloseDatabase(db);
        if (GlyphFontOpen(font, name)) {
            return true;
        }
    }

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
                for (k = 0; k < 8; k++) out[k] = font->extraBytes[i][k];
                return true;
            }
        }
        return false;
    }

    base = GlyphFontGetPage(font, page);
    if (base == 0) {
        return false;
    }
    for (i = 0; i < 8; i++) {
        out[i] = base[offset + i];
    }
    return true;
}

void GlyphFontDrawGlyph(Int16 x, Int16 y, const UInt8 *rows, UInt8 scale)
{
    UInt16 r;
    UInt16 c;
    Int16 right = 159;

    if (rows == 0 || scale == 0) return;

    for (r = 0; r < 8; r++) {
        UInt8 bits = rows[r];

        c = 0;
        while (c < 8) {
            UInt16 start;
            Int16 x0;
            Int16 x1;
            UInt16 yy;

            if ((bits & (UInt8)(0x80 >> c)) == 0) {
                c++;
                continue;
            }

            start = c;
            while (c < 8 && (bits & (UInt8)(0x80 >> c)) != 0) {
                c++;
            }

            x0 = (Int16)(x + (Int16)start * (Int16)scale);
            x1 = (Int16)(x + (Int16)c * (Int16)scale - 1);

            if (x1 < 0 || x0 > right) continue;
            if (x0 < 0) x0 = 0;
            if (x1 > right) x1 = right;

            for (yy = 0; yy < scale; yy++) {
                Int16 sy = (Int16)(y + (Int16)r * (Int16)scale + (Int16)yy);
                if (sy >= 0 && sy < 160) {
                    WinDrawLine(x0, sy, x1, sy);
                }
            }
        }
    }
}

UInt16 GlyphFontDrawGbText(GlyphFont *font, Int16 x, Int16 y,
                           const UInt8 *text, UInt16 byteLen, UInt8 scale)
{
    UInt16 i;
    UInt16 drawn = 0;
    UInt8 rows[8];

    if (!font || !text) return 0;

    i = 0;
    while ((UInt16)(i + 1) < byteLen) {
        UInt16 code = (UInt16)(((UInt16)text[i] << 8) | text[i + 1]);

        if (GlyphFontGet(font, code, rows)) {
            GlyphFontDrawGlyph((Int16)(x + (Int16)drawn * 8 * scale), y, rows, scale);
        }
        drawn++;
        i = (UInt16)(i + 2);
    }
    return drawn;
}
