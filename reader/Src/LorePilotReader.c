/*
 * LorePilotReader.c
 *
 * Entry points for LorePilotReader.
 *
 * Target: Palm OS 2.0 (Pilot Professional). Only Palm OS 1.0/2.x APIs are
 * used; there are no color/high-density/glue calls.
 *
 * Main form is the bookshelf: a list of installed LPBK books with Read/Del
 * buttons, and a list of installed GLYF/LPRD font databases with its own Del
 * button. All tests live in the "Test" menu.
 *
 * Copyright (c) 1999-2000 Palm, Inc. or its subsidiaries.
 * All rights reserved.
 */

#include <PalmOS.h>

#include "LorePilotReader.h"
#include "LorePilotReader_Rsc.h"
#include "GlyphTest.h"
#include "GbTest.h"
#include "FontList.h"
#include "BookList.h"
#include "ReaderForm.h"

/*********************************************************************
 * Internal Constants
 *********************************************************************/

/* Minimum supported OS: Palm OS 2.0. Development stage keeps early 2.0
 * builds (and 2.0 ROMs that report 0x02000000) compatible. */
#define ourMinVersion    sysMakeROMVersion(2,0,0,sysROMStageDevelopment,0)

/*********************************************************************
 * Internal Functions
 *********************************************************************/

static void MainFormRefresh(void)
{
    FormType *frmP = FrmGetActiveForm();

    BookListRefresh(frmP);
    FontListRefresh(frmP);
}

static Boolean MainFormDoCommand(UInt16 command)
{
    Boolean handled = true;

    switch (command)
    {
        case CmdAbout:
            FrmAlert(AboutAlert);
            break;

        case CmdRefreshList:
            MainFormRefresh();
            break;

        case TestGlyph8x8:
            FrmGotoForm(GlyphTestForm);
            break;

        case TestGb8x8:
            GbTestRequest(8);
            FrmGotoForm(GbTestForm);
            break;

        case TestGb12x12:
            GbTestRequest(12);
            FrmGotoForm(GbTestForm);
            break;

        default:
            handled = false;
            break;
    }

    return handled;
}

static void MainFormReadSelected(void)
{
    const BookInfo *book = BookListGetSelected();

    if (book == 0) return;

    if (!ReaderFormRequest(book->dbName, book->cell)) {
        FrmCustomAlert(OpenBookFailedAlert, book->dbName, "", "");
        return;
    }
    FrmGotoForm(ReaderForm);
}

static Boolean MainFormHandleEvent(EventType *eventP)
{
    Boolean handled = false;
    FormType *frmP;

    switch (eventP->eType)
    {
        case frmOpenEvent:
            frmP = FrmGetActiveForm();
            FrmDrawForm(frmP);
            MainFormRefresh();
            handled = true;
            break;

        case frmUpdateEvent:
            FrmDrawForm(FrmGetActiveForm());
            MainFormRefresh();
            handled = true;
            break;

        case menuEvent:
            MenuEraseStatus(0);
            handled = MainFormDoCommand(eventP->data.menu.itemID);
            break;

        case lstSelectEvent:
            if (eventP->data.lstSelect.listID == MainBookList) {
                BookListSelect(FrmGetActiveForm(),
                               eventP->data.lstSelect.selection);
                handled = true;
            } else if (eventP->data.lstSelect.listID == MainFontList) {
                FontListSelect(FrmGetActiveForm(),
                               eventP->data.lstSelect.selection);
                handled = true;
            }
            break;

        case ctlSelectEvent:
            switch (eventP->data.ctlSelect.controlID)
            {
                case MainReadButton:
                    MainFormReadSelected();
                    handled = true;
                    break;

                case MainDeleteBookButton:
                    BookListDeleteSelected(FrmGetActiveForm());
                    handled = true;
                    break;

                case MainDeleteFontButton:
                    FontListDeleteSelected(FrmGetActiveForm());
                    handled = true;
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }

    return handled;
}

static Boolean AppHandleEvent(EventType *eventP)
{
    UInt16 formId;
    FormType *frmP;

    if (eventP->eType == frmLoadEvent)
    {
        formId = eventP->data.frmLoad.formID;
        frmP = FrmInitForm(formId);
        FrmSetActiveForm(frmP);

        switch (formId)
        {
            case MainForm:
                FrmSetEventHandler(frmP, MainFormHandleEvent);
                break;

            case ReaderForm:
                FrmSetEventHandler(frmP, ReaderFormHandleEvent);
                break;

            case GlyphTestForm:
                FrmSetEventHandler(frmP, GlyphTestFormHandleEvent);
                break;

            case GbTestForm:
                FrmSetEventHandler(frmP, GbTestFormHandleEvent);
                break;

            default:
                break;
        }
        return true;
    }

    return false;
}

static void AppEventLoop(void)
{
    UInt16 error;
    EventType event;

    do
    {
        EvtGetEvent(&event, evtWaitForever);

        if (! SysHandleEvent(&event))
        {
            if (! MenuHandleEvent(0, &event, &error))
            {
                if (! AppHandleEvent(&event))
                {
                    FrmDispatchEvent(&event);
                }
            }
        }
    } while (event.eType != appStopEvent);
}

static Err AppStart(void)
{
    return errNone;
}

static void AppStop(void)
{
    FrmCloseAllForms();
}

static Err RomVersionCompatible(UInt32 requiredVersion, UInt16 launchFlags)
{
    UInt32 romVersion;

    romVersion = 0;
    FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);
    if (romVersion == 0)
    {
        /* Older ROMs may not expose the feature; treat them as 2.0. */
        romVersion = ourMinVersion;
    }

    if (romVersion < requiredVersion)
    {
        if ((launchFlags &
            (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
            (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp))
        {
            FrmAlert(RomIncompatibleAlert);
        }

        return sysErrRomIncompatible;
    }

    return errNone;
}

/*********************************************************************
 * Entry point
 *********************************************************************/

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    Err error;

    (void)cmdPBP;

    error = RomVersionCompatible(ourMinVersion, launchFlags);
    if (error) return (error);

    switch (cmd)
    {
        case sysAppLaunchCmdNormalLaunch:
            error = AppStart();
            if (error)
                return error;

            FrmGotoForm(MainForm);
            AppEventLoop();

            AppStop();
            break;

        default:
            break;
    }

    return errNone;
}
