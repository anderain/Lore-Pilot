/*
 * BookList.h
 *
 * Bookshelf model: one row per installed LPBK/LPRD volume.
 *
 * Volumes are standalone files (LPB_<NAME>_<NN>). Each can be installed,
 * opened and deleted on its own, which is the point on a 1 MB Palm: the reader
 * never needs the other volumes of the same book.
 */

#ifndef BOOKLIST_H_
#define BOOKLIST_H_

#include <PalmOS.h>

#define kBookListMax      24
#define kBookNameLen      17
#define kBookDbNameLen    32

typedef struct BookInfoType {
    Char   dbName[kBookDbNameLen];
    Char   displayName[kBookNameLen];
    UInt16 cell;
    UInt16 partIndex;      /* 0-based */
    UInt16 partCount;
    UInt32 firstPage;      /* global page number of this volume's first page */
    UInt32 pageCount;
    UInt32 totalPages;     /* pages in the whole book, for the progress bar */
    UInt32 sizeBytes;
} BookInfo;

/* Rebuild the volume list from installed databases. */
void    BookListRefresh(FormType *formP);

/* Called when the user taps the list. */
void    BookListSelect(FormType *formP, Int16 itemNum);

/* Currently selected entry, or -1. */
Int16   BookListGetSelection(FormType *formP);

/* Selected volume, or 0. */
const BookInfo *BookListGetSelected(void);

/* Delete the selected volume after confirmation.
 * Returns true if it was deleted. */
Boolean BookListDeleteSelected(FormType *formP);

#endif /* BOOKLIST_H_ */
