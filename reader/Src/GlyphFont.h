/*
 * GlyphFont.h
 *
 * Paged GB2312 glyph font access for LorePilotReader.
 * Palm OS 2.0-safe: Data Manager and Memory Manager 1.0 APIs only.
 */

#ifndef GLYPHFONT_H_
#define GLYPHFONT_H_

#include <PalmOS.h>

/* Page cache: 2400 units per page at most need 2-3 GB pages. */
#define kGlyphFontCachePages 3

typedef struct GlyphFontType {
    DmOpenRef db;
    Boolean   open;
    Boolean   paged;             /* version 2 layout */
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
    UInt16    extraCode[128];
    UInt8     extraBytes[128][8];
    Char      name[32];
} GlyphFont;

/* Try LPGB2312F, then LPGB2312B, then a shared name, then any GLYF/LPRD. */
Boolean GlyphFontOpenAny(GlyphFont *font);
Boolean GlyphFontOpen(GlyphFont *font, const Char *name);
void    GlyphFontClose(GlyphFont *font);

/* Copy 8 bytes for a GB2312 double-byte code into out. Returns false for
 * out-of-range codes or blank slots. */
Boolean GlyphFontGet(GlyphFont *font, UInt16 code, UInt8 *out);

/* Draw one 8x8 glyph as horizontal runs (Palm OS 2.0 safe). */
void GlyphFontDrawGlyph(Int16 x, Int16 y, const UInt8 *rows, UInt8 scale);

/* Draw GB2312 text at x/y. Returns number of glyphs drawn. */
UInt16 GlyphFontDrawGbText(GlyphFont *font, Int16 x, Int16 y,
                           const UInt8 *text, UInt16 byteLen, UInt8 scale);

/* Map one GB2312 character to its dense slot index (for debugging). */
Boolean GlyphFontSlot(GlyphFont *font, UInt16 code, UInt16 *page, UInt16 *offset);

#endif /* GLYPHFONT_H_ */
