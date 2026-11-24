/*
 * BookList.c
 *
 * Every LPBK/LPRD volume is one row. Volumes are never merged into a book:
 * the split exists so a 1 MB Palm only has to hold the volume being read.
 *
 *   BOOKA  1/7  1-100
 *
 * The Palm list font is 6 px wide, so keep rows at 26 characters or less.
 */

#include <PalmOS.h>

#include "BookList.h"
#include "TextDb.h"
#include "LorePilotReader_Rsc.h"

#define kBookListWidth  26

static BookInfo gBookInfo[kBookListMax];
static Char     gBookLabels[kBookListMax][kBookListWidth + 1];
static Char    *gBookLabelPtr[kBookListMax];
static Int16    gBookCount = 0;
static Int16    gBookSelected = -1;
static Char     gBookSelectedName[kBookDbNameLen];

static void BookListAppendStr(Char *dst, const Char *src, UInt16 max)
{
    UInt16 len = StrLen(dst);

    while (*src != '\0' && len < (UInt16)(max - 1)) {
        dst[len++] = *src++;
    }
    dst[len] = '\0';
}

static void BookListAppendInt(Char *dst, UInt32 value, UInt16 max)
{
    Char tmp[12];

    StrIToA(tmp, (Int32)value);
    BookListAppendStr(dst, tmp, max);
}

/* "BOOKA  1/7  1-100", truncated to kBookListWidth. */
static void BookListFormat(Int16 i)
{
    Char *out = gBookLabels[i];
    UInt16 len = 0;
    UInt16 nameLen;

    out[0] = '\0';

    nameLen = StrLen(gBookInfo[i].displayName);
    if (nameLen > 10) nameLen = 10;
    while (len < nameLen) {
        out[len] = gBookInfo[i].displayName[len];
        len++;
    }
    out[len] = '\0';
    while (len < 10 && len < kBookListWidth) {
        out[len++] = ' ';
        out[len] = '\0';
    }

    BookListAppendStr(out, "  ", kBookListWidth);
    BookListAppendInt(out, (UInt32)(gBookInfo[i].partIndex + 1), kBookListWidth);
    BookListAppendStr(out, "/", kBookListWidth);
    BookListAppendInt(out, gBookInfo[i].partCount, kBookListWidth);

    BookListAppendStr(out, " ", kBookListWidth);
    BookListAppendInt(out, gBookInfo[i].firstPage, kBookListWidth);
    BookListAppendStr(out, "-", kBookListWidth);
    BookListAppendInt(out,
                      gBookInfo[i].firstPage + gBookInfo[i].pageCount - 1,
                      kBookListWidth);
}

static ListType* BookListGet(FormType *formP)
{
    UInt16 index;

    index = FrmGetObjectIndex(formP, MainBookList);
    return (ListType *)FrmGetObjectPtr(formP, index);
}

/* Sort by book name, then volume number so volumes stay in reading order. */
static Int16 BookListCompare(const BookInfo *a, const BookInfo *b)
{
    Int16 cmp = StrCompare(a->displayName, b->displayName);

    if (cmp != 0) return cmp;
    return (Int16)((Int16)a->partIndex - (Int16)b->partIndex);
}

static void BookListSort(void)
{
    Int16 i;

    for (i = 1; i < gBookCount; i++) {
        BookInfo tmp = gBookInfo[i];
        Int16 j = (Int16)(i - 1);

        while (j >= 0 && BookListCompare(&gBookInfo[j], &tmp) > 0) {
            gBookInfo[j + 1] = gBookInfo[j];
            j--;
        }
        gBookInfo[j + 1] = tmp;
    }
}

void BookListRefresh(FormType *formP)
{
    ListType *list;
    UInt16 total;
    UInt16 i;
    UInt16 buttonIndex;
    ControlType *control;
    Int16 keep = -1;

    list = BookListGet(formP);
    if (list == 0) return;

    gBookCount = 0;
    total = DmNumDatabases(0);

    for (i = 0; i < total && gBookCount < kBookListMax; i++) {
        LocalID dbID = DmGetDatabase(0, i);
        Char name[32];
        UInt32 type = 0;
        UInt32 creator = 0;
        TextDbHeader header;
        UInt32 records = 0;
        UInt32 size = 0;
        UInt32 dataBytes = 0;
        BookInfo *info;

        if (dbID == 0) continue;
        MemSet(name, sizeof(name), 0);
        if (DmDatabaseInfo(0, dbID, name, 0, 0, 0, 0, 0, 0, 0, 0,
                           &type, &creator) != errNone) {
            continue;
        }
        if (type != 'LPBK' || creator != 'LPRD') continue;
        if (!TextDbProbe(name, &header)) continue;

        if (DmDatabaseSize(0, dbID, &records, &size, &dataBytes) != errNone) {
            size = 0;
        }

        info = &gBookInfo[gBookCount];
        MemSet(info, sizeof(*info), 0);
        BookListAppendStr(info->dbName, name, kBookDbNameLen);
        BookListAppendStr(info->displayName, header.displayName, kBookNameLen);
        info->cell = header.cell;
        info->partIndex = header.partIndex;
        info->partCount = header.partCount;
        info->firstPage = header.firstPage;
        info->pageCount = header.pageCount;
        info->totalPages = header.totalPages;
        info->sizeBytes = size;
        gBookCount++;
    }

    BookListSort();

    /* Keep the selection across a refresh by database name, not by row index. */
    keep = -1;
    for (i = 0; i < (UInt16)gBookCount; i++) {
        if (gBookSelectedName[0] != '\0' &&
            StrCompare(gBookInfo[i].dbName, gBookSelectedName) == 0) {
            keep = (Int16)i;
            break;
        }
    }
    if (keep < 0 && gBookCount > 0) keep = 0;

    for (i = 0; i < (UInt16)gBookCount; i++) {
        BookListFormat((Int16)i);
        gBookLabelPtr[i] = gBookLabels[i];
    }

    if (gBookCount == 0) {
        gBookLabelPtr[0] = "No books installed";
        LstSetListChoices(list, gBookLabelPtr, 1);
        LstSetSelection(list, noListSelection);
        gBookSelected = -1;
        gBookSelectedName[0] = '\0';
    } else {
        LstSetListChoices(list, gBookLabelPtr, gBookCount);
        if (keep >= gBookCount) keep = (Int16)(gBookCount - 1);
        LstSetSelection(list, keep);
        gBookSelected = keep;
        BookListAppendStr(gBookSelectedName, gBookInfo[keep].dbName,
                          kBookDbNameLen);
    }
    LstDrawList(list);

    buttonIndex = FrmGetObjectIndex(formP, MainReadButton);
    control = (ControlType *)FrmGetObjectPtr(formP, buttonIndex);
    if (control != 0) {
        CtlSetEnabled(control, (Boolean)(gBookCount > 0));
    }
    buttonIndex = FrmGetObjectIndex(formP, MainDeleteBookButton);
    control = (ControlType *)FrmGetObjectPtr(formP, buttonIndex);
    if (control != 0) {
        CtlSetEnabled(control, (Boolean)(gBookCount > 0));
    }
}

Int16 BookListGetSelection(FormType *formP)
{
    ListType *list = BookListGet(formP);

    if (list == 0) return -1;
    return LstGetSelection(list);
}

void BookListSelect(FormType *formP, Int16 itemNum)
{
    ListType *list = BookListGet(formP);
    UInt16 buttonIndex;
    ControlType *control;

    if (list == 0) return;
    gBookSelected = itemNum;
    gBookSelectedName[0] = '\0';
    if (itemNum >= 0 && itemNum < gBookCount) {
        BookListAppendStr(gBookSelectedName, gBookInfo[itemNum].dbName,
                          kBookDbNameLen);
    }
    LstSetSelection(list, itemNum);
    LstDrawList(list);

    buttonIndex = FrmGetObjectIndex(formP, MainReadButton);
    control = (ControlType *)FrmGetObjectPtr(formP, buttonIndex);
    if (control != 0) {
        CtlSetEnabled(control, (Boolean)(itemNum >= 0 && itemNum < gBookCount));
    }
    buttonIndex = FrmGetObjectIndex(formP, MainDeleteBookButton);
    control = (ControlType *)FrmGetObjectPtr(formP, buttonIndex);
    if (control != 0) {
        CtlSetEnabled(control, (Boolean)(itemNum >= 0 && itemNum < gBookCount));
    }
}

const BookInfo *BookListGetSelected(void)
{
    if (gBookSelected < 0 || gBookSelected >= gBookCount) return 0;
    return &gBookInfo[gBookSelected];
}

Boolean BookListDeleteSelected(FormType *formP)
{
    const BookInfo *book;
    Char name[32];
    LocalID dbID;
    UInt16 choice;

    book = BookListGetSelected();
    if (book == 0) return false;

    MemSet(name, sizeof(name), 0);
    BookListAppendStr(name, book->dbName, sizeof(name));

    choice = FrmCustomAlert(DeleteBookAlert, name, "", "");
    if (choice != 0) return false;         /* Cancel (button index 1) */

    dbID = DmFindDatabase(0, name);
    if (dbID == 0 || DmDeleteDatabase(0, dbID) != errNone) {
        FrmCustomAlert(DeleteBookFailedAlert, name, "", "");
        return false;
    }

    /* The selection is gone; let refresh pick the first remaining row. */
    gBookSelectedName[0] = '\0';
    BookListRefresh(formP);
    return true;
}
