/*
 * UiButton.c
 *
 * Self-drawn button: white face, 1px black frame, centred caption.
 * Pressed state inverts the face and draws the caption in white. Disabled
 * state uses the gray frame and the gray glyph pattern.
 *
 * Captions are either built-in 8x8 Chinese glyphs (UiGlyphs.c) or a short
 * ASCII string drawn with the 6x8 FontHybird font.
 *
 * Only Palm OS 1.0/2.x calls are used.
 */

#include <PalmOS.h>

#include "UiButton.h"
#include "UiFont.h"
#include "UiGlyphs.h"

/* Caption width in pixels, or 0 when there is no caption. */
static Int16 UiButtonCaptionWidth(const UiButton *button)
{
    if (button->text != 0 && button->text[0] != '\0') {
        return UiFontWidth(button->text);
    }
    if (button->label != 0 && button->labelCount != 0) {
        return UiGlyphsTextWidth(button->labelCount);
    }
    return 0;
}

static void UiButtonDrawCaption(const UiButton *button, Int16 x, Int16 y,
                                Boolean inverse)
{
    UInt16 i;

    if (button->text != 0 && button->text[0] != '\0') {
        UiFontDrawText(x, y, button->text, inverse);
        return;
    }

    if (button->label == 0) return;

    for (i = 0; i < button->labelCount; i++) {
        const UiGlyph *glyph = UiGlyphsFind(button->label[i]);

        if (glyph == 0) continue;

        if (!button->enabled) {
            UiFontDrawBitmap8Gray((Int16)(x + (Int16)i * kUiGlyphWidth), y,
                                  glyph->rows);
        } else {
            UiFontDrawBitmap8((Int16)(x + (Int16)i * kUiGlyphWidth), y,
                              glyph->rows, inverse);
        }
    }
}

void UiButtonDraw(const UiButton *button)
{
    RectangleType face;
    Int16 labelW;
    Int16 x;
    Int16 y;

    if (button == 0) return;

    labelW = UiButtonCaptionWidth(button);
    if (labelW == 0) return;

    face = button->bounds;

    if (button->enabled && button->pressed) {
        /* Inverted face, white caption. */
        WinDrawRectangle(&face, 0);
        WinDrawRectangleFrame(simpleFrame, &face);
    } else {
        WinEraseRectangle(&face, 0);
        if (button->enabled) {
            WinDrawRectangleFrame(simpleFrame, &face);
        } else {
            WinDrawGrayRectangleFrame(simpleFrame, &face);
        }
    }

    x = (Int16)(face.topLeft.x + (face.extent.x - labelW) / 2);
    y = (Int16)(face.topLeft.y + (face.extent.y - kUiFontHeight) / 2);

    UiButtonDrawCaption(button, x, y,
                        (Boolean)(button->enabled && button->pressed));
}

Boolean UiButtonHit(const UiButton *button, Int16 x, Int16 y)
{
    if (button == 0 || !button->enabled) return false;
    return RctPtInRectangle(x, y, &button->bounds);
}
