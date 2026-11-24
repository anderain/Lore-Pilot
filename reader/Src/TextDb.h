/*
 * TextDb.h
 *
 * Reader for LPBK/LPRD paginated book databases produced by lore-teller.
 *
 * Record 0 is a 64-byte header, records 1..N hold one page each:
 *
 *   u16 lineCount
 *   u16 reserved
 *   u16 lineEnd[lineCount]   cumulative byte offsets into the text payload
 *   u16 textLen
 *   u8  text[textLen]        GB2312 double bytes
 *
 * Palm OS 2.0-safe: Data Manager 1.0 calls only.
 */

#ifndef TEXTDB_H_
#define TEXTDB_H_

#include <PalmOS.h>

#define kTextDbNameLen      32
#define kTextDbMaxLines     24
/* 17 lines x 20 chars x 2 bytes = 680 for 8px; leave headroom. */
#define kTextDbMaxText      800

typedef struct TextDbHeaderType {
    UInt16 cell;
    UInt16 charsPerLine;
    UInt16 linesPerPage;
    UInt16 headerRows;
    UInt16 footerRows;
    UInt16 progressRows;
    UInt16 partIndex;
    UInt16 partCount;
    UInt16 encoding;
    UInt32 firstPage;
    UInt32 pageCount;
    UInt32 totalPages;
    Char   displayName[17];
} TextDbHeader;

typedef struct TextDbType {
    DmOpenRef db;
    Boolean   open;
    TextDbHeader header;
    Char      name[kTextDbNameLen];
} TextDb;

typedef struct TextDbPageType {
    UInt16 lineCount;
    UInt16 lineStart[kTextDbMaxLines];  /* first byte of each line          */
    UInt16 lineLength[kTextDbMaxLines]; /* bytes in each line               */
    UInt16 textLen;
    UInt8  text[kTextDbMaxText];
} TextDbPage;

/* Read a header without keeping the database open. */
Boolean TextDbProbe(const Char *name, TextDbHeader *header);

Boolean TextDbOpen(TextDb *db, const Char *name);

void    TextDbClose(TextDb *db);

/* Read page `index` (0-based within this part). Returns false on a bad
 * record; the caller should show a blank page. */
Boolean TextDbReadPage(TextDb *db, UInt16 index, TextDbPage *page);

#endif /* TEXTDB_H_ */
