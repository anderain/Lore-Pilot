/*
 * UiGlyphs.h
 *
 * A small built-in 8x8 Chinese glyph set, so the reader's own controls render
 * even when no glyph font database is installed.
 *
 * Glyph bytes are the same format as a GLYF/LPRD glyph PDB: 8 rows, MSB
 * first, 1 = ink. They come from the Fusion Pixel 8px font (SIL OFL 1.1),
 * the same source as the font PDBs.
 *
 * UiGlyphs.c is the single source of truth: lore-teller/scripts/
 * gen-ui-glyphs.mjs reads it and generates the web preview's copy.
 */

#ifndef UIGLYPHS_H_
#define UIGLYPHS_H_

#include <PalmOS.h>

#define kUiGlyphWidth  8
#define kUiGlyphHeight 8

typedef struct UiGlyphType {
    UInt16 code;                 /* GB2312 double-byte code */
    UInt8  rows[kUiGlyphHeight]; /* 8 rows, MSB first, 1 = ink */
} UiGlyph;

/* Number of built-in glyphs. */
UInt16 UiGlyphsCount(void);

/* Index of a code in the table, or -1. */
Int16 UiGlyphsIndex(UInt16 code);

/* Lookup by GB2312 code, or 0 when it is not built in. */
const UiGlyph *UiGlyphsFind(UInt16 code);

/* Draw `count` codes at 8px pitch starting at x. Codes that are not built in
 * still advance the cursor, so a partly-known label keeps its alignment.
 * Returns the number of glyphs actually drawn. */
UInt16 UiGlyphsDrawText(Int16 x, Int16 y, const UInt16 *codes, UInt16 count,
                        Boolean inverse);

/* Width in pixels of `count` 8px cells. */
Int16 UiGlyphsTextWidth(UInt16 count);

#endif /* UIGLYPHS_H_ */
