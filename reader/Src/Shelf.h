/*
 * Shelf.h
 *
 * Self-drawn bookshelf chrome: the two section labels and the three buttons.
 *
 * Why not use the stock LABEL/BUTTON resources? On Palm OS 2.0 the button
 * widget draws its caption with the system font, which has no Chinese glyphs,
 * and a LABEL is repainted by FrmDrawForm with the same limitation. Drawing
 * them ourselves keeps the bookshelf usable with no font database installed,
 * and there is only one redraw path, so the ROM cannot paint over the labels.
 *
 * The two LIST controls stay stock: their rows are ASCII book names.
 */

#ifndef SHELF_H_
#define SHELF_H_

#include <PalmOS.h>

typedef enum ShelfActionType {
    shelfActionNone = 0,
    shelfActionRead,
    shelfActionDeleteBook,
    shelfActionDeleteFont
} ShelfAction;

/* Set up the three buttons. Call once before the form opens. */
void        ShelfInit(void);

/* Draw the labels and the three buttons. Call after FrmDrawForm and after any
 * enabled-state change. */
void        ShelfDraw(void);

void        ShelfSetBookEnabled(Boolean enabled);
void        ShelfSetFontEnabled(Boolean enabled);

/* Handle pen events on the buttons. Returns the action to run on a completed
 * click, shelfActionNone otherwise. Also returns shelfActionNone without
 * consuming the event when the pen is not on a button, so the lists keep
 * working. */
ShelfAction ShelfHandleEvent(EventType *eventP);

#endif /* SHELF_H_ */
