/*
 * UiFont.h
 *
 * 6x8 ASCII text drawing for the LorePilotReader user interface.
 *
 * FontHybird6x8 is a UI font only: never use it to render body text.
 * On black bands the glyphs are drawn in white (inverse = true).
 */

#ifndef UIFONT_H_
#define UIFONT_H_

#include <PalmOS.h>

#define kUiFontWidth   6
#define kUiFontHeight  8

/* Width in pixels of an ASCII string (6 per character). */
Int16 UiFontWidth(const Char *text);

/* Draw one ASCII character. inverse = true draws white pixels (for black
 * bands), false draws black pixels. */
void UiFontDrawGlyph(Int16 x, Int16 y, UInt8 ch, Boolean inverse);

/* Draw ASCII text; characters outside 0x20..0x7E are skipped. */
void UiFontDrawText(Int16 x, Int16 y, const Char *text, Boolean inverse);

#endif /* UIFONT_H_ */
