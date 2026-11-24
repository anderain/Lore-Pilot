/*
 * TextDb.c
 *
 * Page-by-page reader for LPBK/LPRD book databases. The record is copied
 * into the caller's buffer and unlocked immediately, so no handle is held
 * across calls (1 MB device: keep the dynamic heap free).
 */

#include <PalmOS.h>

#include "TextDb.h"

#define kTextDbHeaderLen   64
#define kTextDbRecordStart 1

static UInt16 TextDbBe16(const UInt8 *p)
{
    return (UInt16)(((UInt16)p[0] << 8) | (UInt16)p[1]);
}

static UInt32 TextDbBe32(const UInt8 *p)
{
    return ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) |
           ((UInt32)p[2] << 8)  |  (UInt32)p[3];
}

static void TextDbCopyName(Char *dst, const Char *src, UInt16 max)
{
    UInt16 i = 0;

    while (i < (UInt16)(max - 1) && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* Fill header from the 64-byte record 0 image. */
static Boolean TextDbParseHeader(const UInt8 *hdr, UInt16 recCount,
                                 TextDbHeader *out)
{
    UInt16 i;

    if (hdr[0] != 'L' || hdr[1] != 'P' || hdr[2] != 'T' || hdr[3] != 'X') {
        return false;
    }
    if (TextDbBe16(hdr + 4) != 1) return false;      /* version 1 */

    out->cell         = TextDbBe16(hdr + 8);
    out->charsPerLine = TextDbBe16(hdr + 10);
    out->linesPerPage = TextDbBe16(hdr + 12);
    out->headerRows   = TextDbBe16(hdr + 14);
    out->footerRows   = TextDbBe16(hdr + 16);
    out->partIndex    = TextDbBe16(hdr + 18);
    out->partCount    = TextDbBe16(hdr + 20);
    out->encoding     = TextDbBe16(hdr + 22);
    out->firstPage    = TextDbBe32(hdr + 24);
    out->pageCount    = TextDbBe32(hdr + 28);
    out->totalPages   = TextDbBe32(hdr + 32);
    out->progressRows = TextDbBe16(hdr + 42);

    for (i = 0; i < 16 && hdr[44 + i] != 0; i++) {
        out->displayName[i] = (Char)hdr[44 + i];
    }
    out->displayName[i] = '\0';

    if (out->cell != 8 && out->cell != 12) return false;
    if (out->partCount == 0) return false;
    if (out->pageCount == 0) return false;
    if (recCount < (UInt16)(kTextDbRecordStart + 1)) return false;

    /* Old files have offset 42 as a reserved zero; treat it as "no bar". */
    if (out->progressRows != 0 && out->progressRows > 4) {
        out->progressRows = 0;
    }
    return true;
}

Boolean TextDbProbe(const Char *name, TextDbHeader *header)
{
    LocalID dbID;
    DmOpenRef db;
    MemHandle h;
    const UInt8 *p;
    UInt8 hdr[kTextDbHeaderLen];
    Boolean ok;

    if (name == 0 || header == 0) return false;
    MemSet(header, sizeof(*header), 0);

    dbID = DmFindDatabase(0, name);
    if (dbID == 0) return false;

    db = DmOpenDatabase(0, dbID, dmModeReadOnly);
    if (db == 0) return false;

    h = DmQueryRecord(db, 0);
    if (h == 0 || MemHandleSize(h) < kTextDbHeaderLen) {
        DmCloseDatabase(db);
        return false;
    }
    p = (const UInt8 *)MemHandleLock(h);
    if (p == 0) {
        DmCloseDatabase(db);
        return false;
    }
    MemMove(hdr, p, kTextDbHeaderLen);
    MemHandleUnlock(h);

    ok = TextDbParseHeader(hdr, DmNumRecords(db), header);
    DmCloseDatabase(db);
    return ok;
}

Boolean TextDbOpen(TextDb *db, const Char *name)
{
    LocalID dbID;
    DmOpenRef ref;
    MemHandle h;
    const UInt8 *p;
    UInt8 hdr[kTextDbHeaderLen];

    if (db == 0) return false;
    MemSet(db, sizeof(*db), 0);

    if (name == 0 || name[0] == '\0') return false;

    dbID = DmFindDatabase(0, name);
    if (dbID == 0) return false;

    ref = DmOpenDatabase(0, dbID, dmModeReadOnly);
    if (ref == 0) return false;

    h = DmQueryRecord(ref, 0);
    if (h == 0 || MemHandleSize(h) < kTextDbHeaderLen) {
        DmCloseDatabase(ref);
        return false;
    }
    p = (const UInt8 *)MemHandleLock(h);
    if (p == 0) {
        DmCloseDatabase(ref);
        return false;
    }
    MemMove(hdr, p, kTextDbHeaderLen);
    MemHandleUnlock(h);

    if (!TextDbParseHeader(hdr, DmNumRecords(ref), &db->header)) {
        DmCloseDatabase(ref);
        return false;
    }

    db->db = ref;
    db->open = true;
    TextDbCopyName(db->name, name, kTextDbNameLen);
    return true;
}

void TextDbClose(TextDb *db)
{
    if (db == 0) return;

    if (db->db != 0) {
        DmCloseDatabase(db->db);
    }
    db->db = 0;
    db->open = false;
}

Boolean TextDbReadPage(TextDb *db, UInt16 index, TextDbPage *page)
{
    MemHandle h;
    const UInt8 *p;
    UInt32 size;
    UInt16 rec;
    UInt16 lineCount;
    UInt16 rawLineCount;
    UInt16 i;
    UInt16 textLen;
    UInt16 headerBytes;

    if (db == 0 || page == 0 || !db->open) return false;

    MemSet(page, sizeof(*page), 0);

    rec = (UInt16)(kTextDbRecordStart + index);
    if (rec >= DmNumRecords(db->db)) return false;

    h = DmQueryRecord(db->db, rec);
    if (h == 0) return false;

    size = MemHandleSize(h);
    if (size < 6) return false;

    p = (const UInt8 *)MemHandleLock(h);
    if (p == 0) return false;

    rawLineCount = TextDbBe16(p);

    /* Corrupt records could claim a huge line count; the text offset is
     * computed from it, so reject anything that cannot fit the record. */
    if (rawLineCount > 128) {
        MemHandleUnlock(h);
        return false;
    }

    /* The text payload starts after the real line table, so compute that
     * offset from the raw count even when the table itself is truncated. */
    headerBytes = (UInt16)(4 + rawLineCount * 2 + 2);
    if (size < headerBytes) {
        MemHandleUnlock(h);
        return false;
    }

    lineCount = rawLineCount;
    if (lineCount > kTextDbMaxLines) lineCount = kTextDbMaxLines;

    /* lineEnd[] is cumulative; convert to start/length pairs. */
    {
        UInt16 prev = 0;

        for (i = 0; i < lineCount; i++) {
            UInt16 end = TextDbBe16(p + 4 + i * 2);

            if (end < prev) end = prev;
            page->lineStart[i] = prev;
            page->lineLength[i] = (UInt16)(end - prev);
            prev = end;
        }
        textLen = TextDbBe16(p + 4 + rawLineCount * 2);
        if (textLen > prev && lineCount == rawLineCount) {
            textLen = prev;                  /* trust the line table */
        }
    }

    if (textLen > kTextDbMaxText) textLen = kTextDbMaxText;
    if (size < (UInt32)(headerBytes + textLen)) {
        textLen = (UInt16)(size - headerBytes);
    }

    page->lineCount = lineCount;
    page->textLen = textLen;
    if (textLen > 0) {
        MemMove(page->text, p + headerBytes, textLen);
    }

    MemHandleUnlock(h);
    return true;
}
