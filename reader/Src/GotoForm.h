/*
 * GotoForm.h
 *
 * Self-drawn "jump to page" panel.
 *
 * Why not a FIELD? PilRC emits a FieldAttrType whose bitfield packing differs
 * from the one Palm OS 2.0 uses, so any form containing a FIELD has its object
 * table misread by the ROM (the emulator reports a bogus offscreen object and
 * the buttons disappear). The whole panel is therefore self-drawn: a text box,
 * a 0-9 keypad and two buttons, all with 1.0 traps.
 *
 * The panel edits a global page number and only accepts values inside the
 * volume being read; an empty or out-of-range entry is silently discarded.
 */

#ifndef GOTOFORM_H_
#define GOTOFORM_H_

#include <PalmOS.h>

/* Remember the volume's page range and the current page, then
 * FrmGotoForm(GotoForm). */
void    GotoFormRequest(UInt32 firstPage, UInt32 lastPage, UInt32 currentPage);

/* Called from the reader's frmOpenEvent. Returns true when the user accepted
 * a page; the value is written to *page and the result is cleared. */
Boolean GotoFormTakePage(UInt32 *page);

Boolean GotoFormHandleEvent(EventType *eventP);

#endif /* GOTOFORM_H_ */
