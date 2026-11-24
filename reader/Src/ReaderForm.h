/*
 * ReaderForm.h
 *
 * Full-screen page reader for one LPBK volume.
 *
 * A volume is a standalone file: page turning stops at its first and last
 * page. The header's firstPage/totalPages are still used for the global page
 * number and the progress bar, so the reader knows where it is in the book.
 *
 * Layout matches lore-teller/src/lib/render.ts:
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

/* Open volume `dbName` and load its first page. Called from the bookshelf
 * before switching forms, so no form transition happens inside an event
 * handler. Returns false if the database could not be opened. */
Boolean ReaderFormRequest(const Char *dbName, UInt16 cell);

Boolean ReaderFormHandleEvent(EventType *eventP);

#endif /* READERFORM_H_ */
