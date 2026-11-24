/*
 * ReaderForm.h
 *
 * Full-screen page reader for LPBK book databases.
 *
 * The layout matches lore-teller/src/lib/render.ts:
 *
 *   y 0..7      black band, white 6x8 ASCII: displayName + "N/T"
 *   y 8..149    white body, glyph font, vertically centred
 *   y 150..157  black band, white 6x8 ASCII: PREV  MENU  NEXT
 *   y 158       white progress border
 *   y 159       white progress fill (160 * page / total)
 */

#ifndef READERFORM_H_
#define READERFORM_H_

#include <PalmOS.h>

/* Remember which book to open, then FrmGotoForm(ReaderForm). Loads the first
 * page immediately; returns false if the first part is missing. */
Boolean ReaderFormRequest(const Char *displayName, UInt16 cell);

Boolean ReaderFormHandleEvent(EventType *eventP);

#endif /* READERFORM_H_ */
