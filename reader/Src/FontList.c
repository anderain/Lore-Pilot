/*
 * FontList.c
 *
 * Scans the installed GLYF/LPRD font databases and shows them in a list.
 * Palm OS 2.0-safe: List Manager, Data Manager and Form 1.0 APIs only.
 *
 * Each row shows: name, cell size and size in KB, e.g.
 *   LPF12F 12x12 192K
 */

#include <PalmOS.h>

#include "FontList.h"
#include "GlyphFont.h"
#include "Shelf.h"
#include "LorePilotReader_Rsc.h"

#define kFontListMax    16
#define kFontListWidth  39

static GlyphFontInfo gFontInfo[kFontListMax];
static Char          gFontLabels[kFontListMax][kFontListWidth + 1];
static Char         *gFontLabelPtr[kFontListMax];
static Int16         gFontCount = 0;
static Int16         gFontSelected = -1;

static void FontListAppendStr(Char *dst, const Char *src)
{
    StrCat(dst, src);
}

static void FontListAppendInt(Char *dst, UInt32 value)
{
    Char tmp[12];

    StrIToA(tmp, (Int32)value);
    StrCat(dst, tmp);
}

static void FontListAppendSize(Char *dst, UInt32 bytes)
{
    UInt32 kb = (bytes + 512) / 1024;

    FontListAppendInt(dst, kb);
    FontListAppendStr(dst, "K");
}

/* Local bounded copy: StrNCopy lives at 0xA2CE (a 2.x trap); keep the code
 * on the 1.0 trap range so it also runs on early 2.0 ROMs. */
static void FontListCopy(Char *dst, const Char *src, UInt16 max)
{
    UInt16 i = 0;

    while (i < max && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* Build "NAME 12x12 192K" into gFontLabels[i]. The Palm list font is 6px
 * wide, so keep the row at 24 characters or less (144px < 156px). */
static void FontListFormat(Int16 i)
{
    Char *out = gFontLabels[i];
    UInt16 len;
    UInt16 nameLen;

    out[0] = '\0';

    /* Names are normally 7-8 chars; truncate defensively. */
    nameLen = StrLen(gFontInfo[i].name);
    if (nameLen > 9) nameLen = 9;
    FontListCopy(out, gFontInfo[i].name, nameLen);
    out[nameLen] = '\0';

    len = nameLen;
    while (len < 10) {
        out[len++] = ' ';
        out[len] = '\0';
    }

    FontListAppendInt(out, gFontInfo[i].cellW);
    FontListAppendStr(out, "x");
    FontListAppendInt(out, gFontInfo[i].cellH);
    FontListAppendStr(out, " ");

    FontListAppendSize(out, gFontInfo[i].sizeBytes);
}

static ListType* FontListGet(FormType *formP)
{
    UInt16 index;

    index = FrmGetObjectIndex(formP, MainFontList);
    return (ListType *)FrmGetObjectPtr(formP, index);
}

void FontListRefresh(FormType *formP)
{
    ListType *list;
    UInt16 i;
    Int16 keep;

    list = FontListGet(formP);
    if (list == 0) return;

    gFontCount = (Int16)GlyphFontList(gFontInfo, kFontListMax);
    keep = gFontSelected;

    for (i = 0; i < (UInt16)gFontCount; i++) {
        FontListFormat((Int16)i);
        gFontLabelPtr[i] = gFontLabels[i];
    }

    if (gFontCount == 0) {
        gFontLabelPtr[0] = "No fonts installed";
        LstSetListChoices(list, gFontLabelPtr, 1);
        LstSetSelection(list, noListSelection);
        gFontSelected = -1;
    } else {
        LstSetListChoices(list, gFontLabelPtr, gFontCount);
        if (keep >= gFontCount) keep = (Int16)(gFontCount - 1);
        if (keep < 0) keep = 0;
        LstSetSelection(list, keep);
        gFontSelected = keep;
    }
    LstDrawList(list);

    ShelfSetFontEnabled((Boolean)(gFontCount > 0));
}

Int16 FontListGetSelection(FormType *formP)
{
    ListType *list = FontListGet(formP);

    if (list == 0) return -1;
    return LstGetSelection(list);
}

void FontListSelect(FormType *formP, Int16 itemNum)
{
    ListType *list = FontListGet(formP);

    if (list == 0) return;
    gFontSelected = itemNum;
    LstSetSelection(list, itemNum);
    LstDrawList(list);

    ShelfSetFontEnabled((Boolean)(gFontCount > 0));
}

Boolean FontListDeleteSelected(FormType *formP)
{
    Int16 item = FontListGetSelection(formP);
    Char name[32];
    UInt16 choice;

    if (item < 0 || item >= gFontCount) return false;

    name[0] = '\0';
    FontListCopy(name, gFontInfo[item].name, 31);
    name[31] = '\0';

    choice = FrmCustomAlert(DeleteFontAlert, name, "", "");
    if (choice != 0) return false;         /* Cancel (button index 1) */

    if (DmDeleteDatabase(0, gFontInfo[item].dbID) != errNone) {
        FrmCustomAlert(DeleteFailedAlert, name, "", "");
        return false;
    }

    FontListRefresh(formP);
    return true;
}
