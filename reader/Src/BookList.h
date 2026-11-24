/*
 * BookList.h
 *
 * Bookshelf model: every LPBK/LPRD book database that lore-teller produced.
 *
 * A multi-part book is one entry in the list. Parts are never cached; the
 * reader recomputes LPB_<NAME>_<NN> when it turns across a part boundary.
 */

#ifndef BOOKLIST_H_
#define BOOKLIST_H_

#include <PalmOS.h>

#define kBookListMax      16
#define kBookNameLen      17

typedef struct BookInfoType {
    Char   displayName[kBookNameLen];
    UInt16 cell;
    UInt16 partCount;
    UInt16 progressRows;
    UInt32 totalPages;
    UInt32 sizeBytes;
} BookInfo;

/* Build the database name of one part: LPB_<NAME>_<NN>. */
void    BookListPartName(const Char *displayName, UInt16 partNumber,
                         Char *out, UInt16 max);

/* Rebuild the book list from installed databases. */
void    BookListRefresh(FormType *formP);

/* Called when the user taps the book list. */
void    BookListSelect(FormType *formP, Int16 itemNum);

/* Currently selected entry, or -1. */
Int16   BookListGetSelection(FormType *formP);

/* Selected book, or 0. */
const BookInfo *BookListGetSelected(void);

/* Delete every part of the selected book after confirmation.
 * Returns true if anything was deleted. */
Boolean BookListDeleteSelected(FormType *formP);

#endif /* BOOKLIST_H_ */
