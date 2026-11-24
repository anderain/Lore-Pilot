/*
 * BookList.c
 *
 * Enumerates LPBK/LPRD databases and folds the parts of one book into a
 * single list row:
 *
 *   BOOKA        245p x3
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
static Char     gBookSelectedName[kBookNameLen];

/* Build the database name for one part: LPB_<NAME>_<NN>. */
void BookListPartName(const Char *displayName, UInt16 partNumber,
                      Char *out, UInt16 max)
{
    Char digits[3];
    UInt16 i = 0;
    UInt16 len = 0;
    UInt16 value = partNumber;

    if (max < 5) return;
    if (value < 1) value = 1;
    if (value > 99) value = 99;

    out[len++] = 'L';
    out[len++] = 'P';
    out[len++] = 'B';
    out[len++] = '_';

    for (i = 0; displayName[i] != '\0' && len < (UInt16)(max - 4); i++) {
        out[len++] = displayName[i];
    }

    out[len++] = '_';
    digits[0] = (Char)('0' + (value / 10) % 10);
    digits[1] = (Char)('0' + value % 10);
    if (value >= 10) {
        out[len++] = digits[0];
    }
    out[len++] = digits[1];
    out[len] = '\0';
}

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

/* "BOOKA        245p x3", truncated to kBookListWidth. */
static void BookListFormat(Int16 i)
{
    Char *out = gBookLabels[i];
    UInt16 len;
    UInt16 nameLen;

    out[0] = '\0';

    nameLen = StrLen(gBookInfo[i].displayName);
    if (nameLen > 12) nameLen = 12;
    len = 0;
    while (len < nameLen) {
        out[len] = gBookInfo[i].displayName[len];
        len++;
    }
    out[len] = '\0';
    while (len < 12 && len < kBookListWidth) {
        out[len++] = ' ';
        out[len] = '\0';
    }

    BookListAppendInt(out, gBookInfo[i].totalPages, kBookListWidth);
    BookListAppendStr(out, "p", kBookListWidth);
    if (gBookInfo[i].partCount > 1) {
        BookListAppendStr(out, " x", kBookListWidth);
        BookListAppendInt(out, gBookInfo[i].partCount, kBookListWidth);
    }
}

static ListType* BookListGet(FormType *formP)
{
    UInt16 index;

    index = FrmGetObjectIndex(formP, MainBookList);
    return (ListType *)FrmGetObjectPtr(formP, index);
}

/* Find an existing group for this display name, or -1. */
static Int16 BookListFindGroup(const Char *displayName)
{
    Int16 i;

    for (i = 0; i < gBookCount; i++) {
        if (StrCompare(gBookInfo[i].displayName, displayName) == 0) {
            return i;
        }
    }
    return -1;
}

/* Sort groups by display name so the list order is stable. */
static void BookListSort(void)
{
    Int16 i;

    for (i = 1; i < gBookCount; i++) {
        BookInfo tmp = gBookInfo[i];
        Int16 j = (Int16)(i - 1);

        while (j >= 0 && StrCompare(gBookInfo[j].displayName, tmp.displayName) > 0) {
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
    ControlType *readButton;
    Int16 keep;

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
        Int16 group;
        UInt32 records = 0;
        UInt32 size = 0;
        UInt32 dataBytes = 0;

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

        group = BookListFindGroup(header.displayName);
        if (group < 0) {
            group = gBookCount;
            gBookInfo[group].displayName[0] = '\0';
            BookListAppendStr(gBookInfo[group].displayName, header.displayName,
                              kBookNameLen);
            gBookInfo[group].cell = header.cell;
            gBookInfo[group].partCount = header.partCount;
            gBookInfo[group].progressRows = header.progressRows;
            gBookInfo[group].totalPages = header.totalPages;
            gBookInfo[group].sizeBytes = 0;
            gBookCount++;
        }
        gBookInfo[group].sizeBytes += size;
    }

    BookListSort();

    /* Keep the selection across a refresh by name, not by row index: sorting
     * may reorder the rows when the installed set changes. */
    keep = BookListFindGroup(gBookSelectedName);
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
        gBookSelectedName[0] = '\0';
        BookListAppendStr(gBookSelectedName, gBookInfo[keep].displayName,
                          kBookNameLen);
    }
    LstDrawList(list);

    buttonIndex = FrmGetObjectIndex(formP, MainReadButton);
    readButton = (ControlType *)FrmGetObjectPtr(formP, buttonIndex);
    if (readButton != 0) {
        CtlSetEnabled(readButton, (Boolean)(gBookCount > 0));
    }
    buttonIndex = FrmGetObjectIndex(formP, MainDeleteBookButton);
    readButton = (ControlType *)FrmGetObjectPtr(formP, buttonIndex);
    if (readButton != 0) {
        CtlSetEnabled(readButton, (Boolean)(gBookCount > 0));
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
        BookListAppendStr(gBookSelectedName, gBookInfo[itemNum].displayName,
                          kBookNameLen);
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
    UInt16 part;
    UInt16 deleted = 0;
    UInt16 choice;

    book = BookListGetSelected();
    if (book == 0) return false;

    MemSet(name, sizeof(name), 0);
    BookListAppendStr(name, book->displayName, sizeof(name));

    choice = FrmCustomAlert(DeleteBookAlert, name, "", "");
    if (choice != 0) return false;         /* Cancel (button index 1) */

    for (part = 1; part <= book->partCount && part < 100; part++) {
        Char dbName[32];
        LocalID dbID;

        BookListPartName(book->displayName, part, dbName, sizeof(dbName));
        dbID = DmFindDatabase(0, dbName);
        if (dbID == 0) continue;
        if (DmDeleteDatabase(0, dbID) == errNone) {
            deleted++;
        }
    }

    if (deleted == 0) {
        FrmCustomAlert(DeleteBookFailedAlert, name, "", "");
        return false;
    }

    BookListRefresh(formP);
    return true;
}
