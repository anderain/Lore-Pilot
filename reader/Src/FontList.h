/*
 * FontList.h
 *
 * Installed-font list for the LorePilotReader main form.
 */

#ifndef FONTLIST_H_
#define FONTLIST_H_

#include <PalmOS.h>

/* Rebuild the list from the installed GLYF/LPRD databases. */
void    FontListRefresh(FormType *formP);

/* Delete the currently selected database after confirmation.
 * Returns true if something was deleted. */
Boolean FontListDeleteSelected(FormType *formP);

/* Currently selected entry, or -1. */
Int16   FontListGetSelection(FormType *formP);

/* Called when the user taps the list. */
void    FontListSelect(FormType *formP, Int16 itemNum);

#endif /* FONTLIST_H_ */
