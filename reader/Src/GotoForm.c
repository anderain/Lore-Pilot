/*
 * GotoForm.c
 *
 * Self-drawn "jump to page" panel.
 *
 *   y   8   "jump to page" title
 *   y  20   [ 990                (599-999) ]      text box
 *   y  42   [  7  ] [  8  ] [  9  ]
 *   y  62   [  4  ] [  5  ] [  6  ]
 *   y  82   [  1  ] [  2  ] [  3  ]
 *   y 102   [      0      ] [  <-  ]
 *   y 124   [ OK ]        [ Cancel ]
 *
 * Everything is drawn with UiFont / UiGlyphs / UiButton, so the panel works
 * with no installed font and with no form objects at all (see GotoForm.h for
 * why the FIELD-based version had to go).
 *
 * Only Palm OS 1.0/2.x calls are used.
 */

#include <PalmOS.h>

#include "GotoForm.h"
#include "UiButton.h"
#include "UiFont.h"
#include "UiGlyphs.h"
#include "LorePilotReader_Rsc.h"

#define kGotoW            160
#define kGotoH            160

/* Layout. */
#define kGotoTitleY       8
#define kGotoBoxX         12
#define kGotoBoxY         20
#define kGotoBoxW         136
#define kGotoBoxH         18

#define kGotoKeyW         44
#define kGotoKeyH         18
#define kGotoKeyX0        12
#define kGotoKeyX1        58
#define kGotoKeyX2        104
#define kGotoRow0         42
#define kGotoRowStep      20

#define kGotoZeroW        90
#define kGotoZeroY        102

#define kGotoOkX          20
#define kGotoOkY          124
#define kGotoOkW          56
#define kGotoCancelX      84
#define kGotoCancelW      56

/* At most 5 digits: the largest page number is 65535. */
#define kGotoMaxDigits    5

/* built-in label */
static const UInt16 kGotoTitle[] = { 0xCCF8, 0xD7AA, 0xD6C1, 0xD2B3, 0xC2EB };
/* built-in label */
static const UInt16 kGotoLabelOk[] = { 0xCCF8, 0xD7AA };
/* built-in label */
static const UInt16 kGotoLabelCancel[] = { 0xC8A1, 0xCFFB };

static const Char *const kGotoDigitText[10] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

/* Keys 1..9 are laid out 7 8 9 / 4 5 6 / 1 2 3; index maps digit -> key. */
static UiButton gGotoDigitKeys[10];   /* 0..9; [0] is the wide bottom key */
static UiButton gGotoBackKey;
static UiButton gGotoOkKey;
static UiButton gGotoCancelKey;
static UiButton *gGotoKeys[12];
static UInt16    gGotoKeyCount;

static Char    gGotoText[kGotoMaxDigits + 1];
static UInt16  gGotoLen;
static UInt32  gGotoFirst;
static UInt32  gGotoLast;
static UInt32  gGotoResult;
static Boolean gGotoHasResult;

/*------------------------------------------------------------------ helpers*/

static void GotoAppendStr(Char *dst, const Char *src, UInt16 max)
{
    UInt16 len = StrLen(dst);

    while (*src != '\0' && len < (UInt16)(max - 1)) {
        dst[len++] = *src++;
    }
    dst[len] = '\0';
}

static void GotoAppendInt(Char *dst, UInt32 value, UInt16 max)
{
    Char tmp[12];

    StrIToA(tmp, (Int32)value);
    GotoAppendStr(dst, tmp, max);
}

static void GotoSetRect(RectangleType *r, Int16 x, Int16 y, Int16 w, Int16 h)
{
    r->topLeft.x = x;
    r->topLeft.y = y;
    r->extent.x = w;
    r->extent.y = h;
}

static void GotoInitKey(UiButton *button, Int16 x, Int16 y, Int16 w,
                        const Char *text)
{
    GotoSetRect(&button->bounds, x, y, w, kGotoKeyH);
    button->label = 0;
    button->labelCount = 0;
    button->text = text;
    button->enabled = true;
    button->pressed = false;
}

/*------------------------------------------------------------------ drawing*/

static void GotoDrawTitle(void)
{
    Int16 width = UiGlyphsTextWidth((UInt16)(sizeof(kGotoTitle) / sizeof(UInt16)));

    UiGlyphsDrawText((Int16)((kGotoW - width) / 2), kGotoTitleY, kGotoTitle,
                     (UInt16)(sizeof(kGotoTitle) / sizeof(UInt16)), false);
}

static void GotoDrawBox(void)
{
    RectangleType box;
    Char range[24];
    Int16 rangeW;
    Int16 y;

    GotoSetRect(&box, kGotoBoxX, kGotoBoxY, kGotoBoxW, kGotoBoxH);
    WinEraseRectangle(&box, 0);
    WinDrawRectangleFrame(simpleFrame, &box);

    y = (Int16)(kGotoBoxY + (kGotoBoxH - kUiFontHeight) / 2);

    /* Typed digits, left aligned with a blinking-free underscore cursor. */
    UiFontDrawText((Int16)(kGotoBoxX + 4), y, gGotoText, false);
    if (gGotoLen < kGotoMaxDigits) {
        UiFontDrawText((Int16)(kGotoBoxX + 4 + gGotoLen * kUiFontWidth), y,
                       "_", false);
    }

    /* "599-999" on the right: the range this volume holds. */
    range[0] = '\0';
    GotoAppendInt(range, gGotoFirst, sizeof(range));
    GotoAppendStr(range, "-", sizeof(range));
    GotoAppendInt(range, gGotoLast, sizeof(range));

    rangeW = UiFontWidth(range);
    if (rangeW < kGotoBoxW - 8) {
        UiFontDrawText((Int16)(kGotoBoxX + kGotoBoxW - 4 - rangeW), y, range,
                       false);
    }
}

static void GotoDrawAll(void)
{
    RectangleType full;
    UInt16 i;

    GotoSetRect(&full, 0, 0, kGotoW, kGotoH);
    WinEraseRectangle(&full, 0);

    GotoDrawTitle();
    GotoDrawBox();

    for (i = 0; i < gGotoKeyCount; i++) {
        UiButtonDraw(gGotoKeys[i]);
    }
}

/* Redraw just the text box after a key press. */
static void GotoDrawBoxOnly(void)
{
    GotoDrawBox();
}

/*------------------------------------------------------------------ editing*/

static void GotoClearAll(void)
{
    UInt16 i;

    gGotoLen = 0;
    gGotoText[0] = '\0';
    for (i = 0; i < gGotoKeyCount; i++) {
        gGotoKeys[i]->pressed = false;
    }
}

static void GotoAddDigit(UInt16 digit)
{
    if (gGotoLen >= kGotoMaxDigits) return;

    gGotoText[gGotoLen++] = (Char)('0' + digit);
    gGotoText[gGotoLen] = '\0';
}

static void GotoBackspace(void)
{
    if (gGotoLen == 0) return;
    gGotoLen--;
    gGotoText[gGotoLen] = '\0';
}

/* Parse gGotoText; false when empty. */
static Boolean GotoParse(UInt32 *out)
{
    UInt32 value = 0;
    UInt16 i;

    if (gGotoLen == 0) return false;

    for (i = 0; i < gGotoLen; i++) {
        value = value * 10u + (UInt32)(gGotoText[i] - '0');
    }
    *out = value;
    return true;
}

/*------------------------------------------------------------------ keys*/

static UiButton *GotoKeyAt(Int16 x, Int16 y)
{
    UInt16 i;

    for (i = 0; i < gGotoKeyCount; i++) {
        if (UiButtonHit(gGotoKeys[i], x, y)) return gGotoKeys[i];
    }
    return 0;
}

/* Digit of a key, or -1 for the non-digit keys. */
static Int16 GotoKeyDigit(const UiButton *button)
{
    UInt16 i;

    for (i = 0; i < 10; i++) {
        if (&gGotoDigitKeys[i] == button) return (Int16)i;
    }
    return -1;
}

/*------------------------------------------------------------------ build*/

static void GotoBuildKeys(void)
{
    UInt16 digit;

    gGotoKeyCount = 0;

    /* 7 8 9 */
    GotoInitKey(&gGotoDigitKeys[7], kGotoKeyX0, kGotoRow0, kGotoKeyW, "7");
    GotoInitKey(&gGotoDigitKeys[8], kGotoKeyX1, kGotoRow0, kGotoKeyW, "8");
    GotoInitKey(&gGotoDigitKeys[9], kGotoKeyX2, kGotoRow0, kGotoKeyW, "9");

    /* 4 5 6 */
    GotoInitKey(&gGotoDigitKeys[4], kGotoKeyX0, kGotoRow0 + kGotoRowStep,
                kGotoKeyW, "4");
    GotoInitKey(&gGotoDigitKeys[5], kGotoKeyX1, kGotoRow0 + kGotoRowStep,
                kGotoKeyW, "5");
    GotoInitKey(&gGotoDigitKeys[6], kGotoKeyX2, kGotoRow0 + kGotoRowStep,
                kGotoKeyW, "6");

    /* 1 2 3 */
    GotoInitKey(&gGotoDigitKeys[1], kGotoKeyX0, kGotoRow0 + 2 * kGotoRowStep,
                kGotoKeyW, "1");
    GotoInitKey(&gGotoDigitKeys[2], kGotoKeyX1, kGotoRow0 + 2 * kGotoRowStep,
                kGotoKeyW, "2");
    GotoInitKey(&gGotoDigitKeys[3], kGotoKeyX2, kGotoRow0 + 2 * kGotoRowStep,
                kGotoKeyW, "3");

    /* 0 (double width) and backspace */
    GotoInitKey(&gGotoDigitKeys[0], kGotoKeyX0, kGotoZeroY, kGotoZeroW, "0");
    GotoInitKey(&gGotoBackKey, kGotoKeyX2, kGotoZeroY, kGotoKeyW, "<-");

    /* OK / Cancel */
    GotoInitKey(&gGotoOkKey, kGotoOkX, kGotoOkY, kGotoOkW, 0);
    gGotoOkKey.label = kGotoLabelOk;
    gGotoOkKey.labelCount =
        (UInt16)(sizeof(kGotoLabelOk) / sizeof(UInt16));

    GotoInitKey(&gGotoCancelKey, kGotoCancelX, kGotoOkY, kGotoCancelW, 0);
    gGotoCancelKey.label = kGotoLabelCancel;
    gGotoCancelKey.labelCount =
        (UInt16)(sizeof(kGotoLabelCancel) / sizeof(UInt16));

    for (digit = 0; digit < 10; digit++) {
        gGotoKeys[gGotoKeyCount++] = &gGotoDigitKeys[digit];
    }
    gGotoKeys[gGotoKeyCount++] = &gGotoBackKey;
    gGotoKeys[gGotoKeyCount++] = &gGotoOkKey;
    gGotoKeys[gGotoKeyCount++] = &gGotoCancelKey;
}

/*------------------------------------------------------------------ public*/

void GotoFormRequest(UInt32 firstPage, UInt32 lastPage, UInt32 currentPage)
{
    Char start[12];

    gGotoFirst = firstPage;
    gGotoLast = lastPage;
    gGotoHasResult = false;
    gGotoResult = 0;

    GotoClearAll();

    /* Prefill the current page so the reader can see where they are. */
    start[0] = '\0';
    GotoAppendInt(start, currentPage, sizeof(start));
    if (StrLen(start) <= kGotoMaxDigits) {
        GotoAppendStr(gGotoText, start, sizeof(gGotoText));
        gGotoLen = StrLen(gGotoText);
    }

    GotoBuildKeys();
}

Boolean GotoFormTakePage(UInt32 *page)
{
    if (!gGotoHasResult) return false;

    gGotoHasResult = false;
    if (page != 0) *page = gGotoResult;
    return true;
}

/* Accept the typed page if it is inside the volume; otherwise just leave. */
static void GotoAccept(void)
{
    UInt32 page;

    if (GotoParse(&page) && page >= gGotoFirst && page <= gGotoLast) {
        gGotoResult = page;
        gGotoHasResult = true;
    }
}

static Boolean GotoHandlePen(const EventType *eventP)
{
    UiButton *button;
    Int16 digit;

    if (eventP->eType == penDownEvent) {
        button = GotoKeyAt(eventP->screenX, eventP->screenY);
        /* Taps off the keypad (title, text box, margins) are ignored. */
        if (button == 0) return true;
        button->pressed = true;
        UiButtonDraw(button);
        return true;
    }

    /* penUpEvent: fire only when the pen is still over the pressed key. */
    button = GotoKeyAt(eventP->data.penUp.end.x, eventP->data.penUp.end.y);
    if (button == 0) {
        UInt16 i;

        for (i = 0; i < gGotoKeyCount; i++) {
            if (gGotoKeys[i]->pressed) {
                gGotoKeys[i]->pressed = false;
                UiButtonDraw(gGotoKeys[i]);
            }
        }
        return true;
    }

    if (!button->pressed) return true;
    button->pressed = false;
    UiButtonDraw(button);

    if (button == &gGotoOkKey) {
        GotoAccept();
        FrmGotoForm(ReaderForm);
        return true;
    }

    if (button == &gGotoCancelKey) {
        FrmGotoForm(ReaderForm);
        return true;
    }

    if (button == &gGotoBackKey) {
        GotoBackspace();
        GotoDrawBoxOnly();
        return true;
    }

    digit = GotoKeyDigit(button);
    if (digit >= 0) {
        GotoAddDigit((UInt16)digit);
        GotoDrawBoxOnly();
    }
    return true;
}

static Boolean GotoHandleKey(const EventType *eventP)
{
    UInt16 chr = eventP->data.keyDown.chr;

    if (chr >= '0' && chr <= '9') {
        GotoAddDigit((UInt16)(chr - '0'));
        GotoDrawBoxOnly();
        return true;
    }

    if (chr == chrBackspace) {
        GotoBackspace();
        GotoDrawBoxOnly();
        return true;
    }

    if (chr == chrLineFeed || chr == chrCarriageReturn) {
        GotoAccept();
        FrmGotoForm(ReaderForm);
        return true;
    }

    if (chr == chrEscape) {
        FrmGotoForm(ReaderForm);
        return true;
    }

    return false;
}

Boolean GotoFormHandleEvent(EventType *eventP)
{
    Boolean handled = false;

    switch (eventP->eType) {
        case frmOpenEvent:
            FrmDrawForm(FrmGetActiveForm());
            GotoDrawAll();
            handled = true;
            break;

        case frmUpdateEvent:
            GotoDrawAll();
            handled = true;
            break;

        case penDownEvent:
        case penUpEvent:
            handled = GotoHandlePen(eventP);
            break;

        case keyDownEvent:
            handled = GotoHandleKey(eventP);
            break;

        default:
            break;
    }

    return handled;
}
