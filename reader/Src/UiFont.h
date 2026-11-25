/*
 * UiFont.h
 *
 * Text drawing for the LorePilotReader user interface.
 *
 * Two sources of glyphs, both drawn as horizontal runs with WinDrawLine /
 * WinEraseLine (Palm OS 1.0 traps only):
 *
 *   - FontHybird6x8, 6x8 ASCII, for the header and any ASCII label;
 *   - UiGlyphs.c, a small built-in 8x8 Chinese set, so the reader's own
 *     controls work even when no glyph font database is installed.
 *
 * Never use either for body text: the body always comes from a glyph PDB.
 */

#ifndef UIFONT_H_
#define UIFONT_H_

#include <PalmOS.h>

#define kUiFontWidth     6
#define kUiFontHeight    8

#define kUiFontScreenW   160
#define kUiFontScreenH   160

/* Width in pixels of an ASCII string (6 per character). */
Int16 UiFontWidth(const Char *text);

/* Draw one 8-pixel-wide bitmap at x/y. `rows` holds 8 bytes, MSB first,
 * 1 = ink. inverse = true draws white pixels (for black bands). */
void UiFontDrawBitmap8(Int16 x, Int16 y, const UInt8 *rows, Boolean inverse);

/* Same, but in the gray pattern used for disabled controls. */
void UiFontDrawBitmap8Gray(Int16 x, Int16 y, const UInt8 *rows);

/* Draw one ASCII character. inverse = true draws white pixels (for black
 * bands), false draws black pixels. */
void UiFontDrawGlyph(Int16 x, Int16 y, UInt8 ch, Boolean inverse);

/* Draw ASCII text; characters outside 0x20..0x7E are skipped. */
void UiFontDrawText(Int16 x, Int16 y, const Char *text, Boolean inverse);

#endif /* UIFONT_H_ */
