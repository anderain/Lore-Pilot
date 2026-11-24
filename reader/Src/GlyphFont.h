/*
 * GlyphFont.h
 *
 * Paged GB2312 glyph font access for LorePilotReader.
 * Palm OS 2.0-safe: Data Manager and Memory Manager 1.0 APIs only.
 */

#ifndef GLYPHFONT_H_
#define GLYPHFONT_H_

#include <PalmOS.h>

/* Page cache: a 12x12 page is 12KB, so three pages still fit comfortably. */
#define kGlyphFontCachePages 3

/* Largest supported glyph: 12x12 needs 24 bytes (2 per row). */
#define kGlyphFontMaxGlyphBytes 24
#define kGlyphFontMaxExtras     128

typedef struct GlyphFontType {
    DmOpenRef db;
    Boolean   open;
    UInt16    version;
    UInt16    cellW;
    UInt16    cellH;
    UInt16    bytesPerGlyph;
    UInt16    glyphCount;
    UInt16    firstRow, lastRow;
    UInt16    firstCol, lastCol;
    UInt16    rows, cols;
    UInt16    pageCount;
    UInt16    pageGlyphs;
    UInt16    cachedPage[kGlyphFontCachePages];
    MemHandle cachedHandle[kGlyphFontCachePages];
    UInt8*    cachedPtr[kGlyphFontCachePages];
    UInt16    nextVictim;
    UInt16    extraRecord;       /* record holding out-of-grid glyphs, 0 if none */
    UInt16    extraCount;
    UInt16    extraCode[kGlyphFontMaxExtras];
    UInt8     extraBytes[kGlyphFontMaxExtras][kGlyphFontMaxGlyphBytes];
    Char      name[32];
} GlyphFont;

/* Short description used by the font list. */
typedef struct GlyphFontInfoType {
    LocalID  dbID;
    Char     name[32];
    UInt16   cellW;
    UInt16   cellH;
    UInt16   glyphCount;
    UInt16   pageCount;
    UInt32   sizeBytes;
    Boolean  valid;
} GlyphFontInfo;

Boolean GlyphFontOpen(GlyphFont *font, const Char *name);
Boolean GlyphFontOpenAny(GlyphFont *font);

/* Find the best database for a cell size (8 or 12). Prefers more glyphs. */
Boolean GlyphFontFind(GlyphFont *font, UInt8 cell);

/* Read header fields of a database without keeping it open. */
Boolean GlyphFontProbe(const Char *name, GlyphFontInfo *info);

/* Enumerate every GLYF/LPRD font database into info[]; returns the count. */
UInt16 GlyphFontList(GlyphFontInfo *info, UInt16 maxEntries);

void    GlyphFontClose(GlyphFont *font);

/* Copy bytesPerGlyph bytes for a GB2312 code. Returns false for out-of-range
 * codes or blank slots. out must hold at least kGlyphFontMaxGlyphBytes. */
Boolean GlyphFontGet(GlyphFont *font, UInt16 code, UInt8 *out);

/* Draw one glyph as horizontal runs (Palm OS 2.0 safe). */
void GlyphFontDrawGlyph(const GlyphFont *font, Int16 x, Int16 y,
                        const UInt8 *rows, UInt8 scale);

/* Draw GB2312 text at x/y. Returns number of glyphs drawn. */
UInt16 GlyphFontDrawGbText(GlyphFont *font, Int16 x, Int16 y,
                           const UInt8 *text, UInt16 byteLen, UInt8 scale);

/* Map one GB2312 character to its dense slot index (for debugging). */
Boolean GlyphFontSlot(GlyphFont *font, UInt16 code, UInt16 *page, UInt16 *offset);

#endif /* GLYPHFONT_H_ */
