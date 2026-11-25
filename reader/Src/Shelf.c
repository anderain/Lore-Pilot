/*
 * Shelf.c
 *
 * Draws the bookshelf's Chinese chrome and handles its button clicks.
 *
 * Labels:  "bookshelf" and "fonts" (built-in glyphs)
 * Buttons: "read" and "delete" (built-in glyphs)
 *
 * All glyphs come from UiGlyphs.c, so nothing here needs an installed font.
 * The event contract with MainForm is:
 *
 *   - ShelfHandleEvent is called for penDown/penUp BEFORE FrmDispatchEvent;
 *   - it returns shelfActionNone for pens outside the buttons so the list
 *     still receives its own taps.
 */

#include <PalmOS.h>

#include "Shelf.h"
#include "UiButton.h"
#include "UiFont.h"
#include "UiGlyphs.h"
#include "LorePilotReader_Rsc.h"

/* Layout, form-relative. Matches reader/Rsc/LorePilotReader_Rsc.rcp. */
#define kShelfBooksLabelX   2
#define kShelfBooksLabelY   16
#define kShelfFontsLabelX   2
#define kShelfFontsLabelY   88

#define kShelfReadX         2
#define kShelfReadY         74
#define kShelfReadW         46
#define kShelfDeleteBookX   52
#define kShelfDeleteBookY   74
#define kShelfDeleteBookW   30
#define kShelfDeleteFontX   2
#define kShelfDeleteFontY   135
#define kShelfDeleteFontW   30
#define kShelfButtonH       13

/* built-in label */
static const UInt16 kLabelBooks[] = { 0xCAE9, 0xBCDC };
/* built-in label */
static const UInt16 kLabelFonts[] = { 0xD7D6, 0xCCE5 };
/* built-in label */
static const UInt16 kLabelRead[] = { 0xD4C4, 0xB6C1 };
/* built-in label */
static const UInt16 kLabelDelete[] = { 0xC9BE, 0xB3FD };

static UiButton gReadButton;
static UiButton gDeleteBookButton;
static UiButton gDeleteFontButton;

/* The erase rectangle must cover the frame too. */
static void ShelfSetRect(RectangleType *r, Int16 x, Int16 y, Int16 w, Int16 h)
{
    r->topLeft.x = x;
    r->topLeft.y = y;
    r->extent.x = w;
    r->extent.y = h;
}

void ShelfInit(void)
{
    ShelfSetRect(&gReadButton.bounds, kShelfReadX, kShelfReadY,
                 kShelfReadW, kShelfButtonH);
    gReadButton.label = kLabelRead;
    gReadButton.labelCount = (UInt16)(sizeof(kLabelRead) / sizeof(UInt16));
    gReadButton.enabled = false;
    gReadButton.pressed = false;

    ShelfSetRect(&gDeleteBookButton.bounds, kShelfDeleteBookX,
                 kShelfDeleteBookY, kShelfDeleteBookW, kShelfButtonH);
    gDeleteBookButton.label = kLabelDelete;
    gDeleteBookButton.labelCount = (UInt16)(sizeof(kLabelDelete) / sizeof(UInt16));
    gDeleteBookButton.enabled = false;
    gDeleteBookButton.pressed = false;

    ShelfSetRect(&gDeleteFontButton.bounds, kShelfDeleteFontX,
                 kShelfDeleteFontY, kShelfDeleteFontW, kShelfButtonH);
    gDeleteFontButton.label = kLabelDelete;
    gDeleteFontButton.labelCount = (UInt16)(sizeof(kLabelDelete) / sizeof(UInt16));
    gDeleteFontButton.enabled = false;
    gDeleteFontButton.pressed = false;
}

void ShelfDraw(void)
{
    UiGlyphsDrawText(kShelfBooksLabelX, kShelfBooksLabelY, kLabelBooks,
                     (UInt16)(sizeof(kLabelBooks) / sizeof(UInt16)), false);
    UiGlyphsDrawText(kShelfFontsLabelX, kShelfFontsLabelY, kLabelFonts,
                     (UInt16)(sizeof(kLabelFonts) / sizeof(UInt16)), false);

    UiButtonDraw(&gReadButton);
    UiButtonDraw(&gDeleteBookButton);
    UiButtonDraw(&gDeleteFontButton);
}

void ShelfSetBookEnabled(Boolean enabled)
{
    if (gReadButton.enabled == enabled &&
        gDeleteBookButton.enabled == enabled) {
        return;
    }
    gReadButton.enabled = enabled;
    gDeleteBookButton.enabled = enabled;
    UiButtonDraw(&gReadButton);
    UiButtonDraw(&gDeleteBookButton);
}

void ShelfSetFontEnabled(Boolean enabled)
{
    if (gDeleteFontButton.enabled == enabled) return;
    gDeleteFontButton.enabled = enabled;
    UiButtonDraw(&gDeleteFontButton);
}

/* Which button is under the pen, or 0. */
static UiButton *ShelfButtonAt(Int16 x, Int16 y)
{
    if (UiButtonHit(&gReadButton, x, y)) return &gReadButton;
    if (UiButtonHit(&gDeleteBookButton, x, y)) return &gDeleteBookButton;
    if (UiButtonHit(&gDeleteFontButton, x, y)) return &gDeleteFontButton;
    return 0;
}

ShelfAction ShelfHandleEvent(EventType *eventP)
{
    UiButton *button;

    switch (eventP->eType) {
        case penDownEvent:
            button = ShelfButtonAt(eventP->screenX, eventP->screenY);
            if (button == 0) return shelfActionNone;
            button->pressed = true;
            UiButtonDraw(button);
            return shelfActionNone;

        case penUpEvent:
            button = ShelfButtonAt(eventP->data.penUp.end.x,
                                   eventP->data.penUp.end.y);
            if (button != 0 && button->pressed) {
                button->pressed = false;
                UiButtonDraw(button);

                if (button == &gReadButton) return shelfActionRead;
                if (button == &gDeleteBookButton) return shelfActionDeleteBook;
                return shelfActionDeleteFont;
            }

            /* Cancel any press that ended elsewhere. */
            if (gReadButton.pressed) {
                gReadButton.pressed = false;
                UiButtonDraw(&gReadButton);
            }
            if (gDeleteBookButton.pressed) {
                gDeleteBookButton.pressed = false;
                UiButtonDraw(&gDeleteBookButton);
            }
            if (gDeleteFontButton.pressed) {
                gDeleteFontButton.pressed = false;
                UiButtonDraw(&gDeleteFontButton);
            }
            return shelfActionNone;

        default:
            return shelfActionNone;
    }
}
