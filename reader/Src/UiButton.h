/*
 * UiButton.h
 *
 * A tiny self-drawn push button.
 *
 * The stock Palm 2.0 button widget draws its label with the system font, so it
 * cannot show a Chinese label without an installed CJK font. Drawing the
 * buttons ourselves keeps the bookshelf and the goto panel readable with no
 * font database at all, and gives us a single, predictable redraw path (the
 * ROM never repaints over the label).
 *
 * A button carries either a run of built-in Chinese glyph codes (`label`) or a
 * short ASCII string (`text`). ASCII uses the 6x8 FontHybird font.
 *
 * Cost per redraw is small: one frame plus 8-16 WinDrawLine calls per glyph.
 */

#ifndef UIBUTTON_H_
#define UIBUTTON_H_

#include <PalmOS.h>

typedef struct UiButtonType {
    RectangleType  bounds;
    const UInt16  *label;       /* built-in glyph codes (UiGlyphs.c), or 0 */
    UInt16         labelCount;
    const Char    *text;        /* ASCII caption (6x8), or 0 */
    Boolean        enabled;
    Boolean        pressed;     /* only meaningful while enabled */
} UiButton;

/* Erase, draw the caption and the frame. Disabled buttons use the gray
 * pattern. Does nothing when both `label` and `text` are empty. */
void    UiButtonDraw(const UiButton *button);

/* RctPtInRectangle test that also requires the button to be enabled. */
Boolean UiButtonHit(const UiButton *button, Int16 x, Int16 y);

#endif /* UIBUTTON_H_ */
