/*
 * LorePilotReader.c
 *
 * Entry points for LorePilotReader.
 *
 * Target: Palm OS 2.0 (Pilot Professional). Only Palm OS 1.0 APIs are used;
 * there are no color/high-density/glue calls.
 *
 * Copyright (c) 1999-2000 Palm, Inc. or its subsidiaries.
 * All rights reserved.
 */

#include <PalmOS.h>

#include "LorePilotReader.h"
#include "LorePilotReader_Rsc.h"
#include "GlyphTest.h"
#include "GbTest.h"

/*********************************************************************
 * Internal Constants
 *********************************************************************/

/* Minimum supported OS: Palm OS 2.0. Development stage keeps early 2.0
 * builds (and 2.0 ROMs that report 0x02000000) compatible. */
#define ourMinVersion    sysMakeROMVersion(2,0,0,sysROMStageDevelopment,0)

/*********************************************************************
 * Internal Functions
 *********************************************************************/

static void MainFormInit(void)
{
    /* No fields on this form; nothing to focus. */
}

static Boolean MainFormDoCommand(UInt16 command)
{
    Boolean handled = true;

    switch (command)
    {
        case MainAboutButton:
        {
            FrmAlert(AboutAlert);
            break;
        }

        case MainGlyphTestButton:
        {
            FrmGotoForm(GlyphTestForm);
            break;
        }

        case MainGbTestButton:
        {
            FrmGotoForm(GbTestForm);
            break;
        }

        default:
            handled = false;
            break;
    }

    return handled;
}

static Boolean MainFormHandleEvent(EventType *eventP)
{
    Boolean handled = false;

    switch (eventP->eType)
    {
        case frmOpenEvent:
            MainFormInit();
            FrmDrawForm(FrmGetActiveForm());
            handled = true;
            break;

        case frmUpdateEvent:
            FrmDrawForm(FrmGetActiveForm());
            handled = true;
            break;

        case ctlSelectEvent:
            handled = MainFormDoCommand(eventP->data.ctlSelect.controlID);
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
