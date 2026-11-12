#include <avkon.hrh>
#include <aknnotewrappers.h>
#include <aknquerydialog.h>
#include <eikmenup.h>
#include <eikapp.h>
#include <eikspane.h>
#include <bautils.h>

#include <TgPager.rsg>

#include "tgpager.pan"
#include "tgpagerappui.h"
#include "tgpagerappview.h"
#include "tgpager.hrh"
#include "tgstrings.h"
#include "tgpagerengine.h"
#include "tgsettings.h"

void CTgPagerAppUi::ConstructL()
    {
    BaseConstructL();

    StatusPane()->MakeVisible(EFalse); // без заголовка сверху — больше места
    iAppView = CTgPagerAppView::NewL(ClientRect());
    AddToStackL(iAppView);

    iSettings = CTgSettings::NewL();
    LoadSettingsL();

    iEngine = CTgEngine::NewL(*this);
    iEngine->Configure(iSettings->iHost, iSettings->iPort, iSettings->iCode);

    iAppView->AddSysL(TGSTR(KStMenuHint));

    if (iSettings->iHost.Length() > 0)
        iEngine->Connect();
    }

CTgPagerAppUi::CTgPagerAppUi()
    {
    iAppView = NULL;
    iEngine = NULL;
    iSettings = NULL;
    iWasConnected = EFalse;
    iDownShown = EFalse;
    }

CTgPagerAppUi::~CTgPagerAppUi()
    {
    if (iAppView)
        RemoveFromStack(iAppView);
    delete iEngine;
    delete iSettings;
    delete iAppView;
    }

TBool CTgPagerAppUi::AppDir(TFileName& aDir)
    {
    CEikApplication* app = Application();
    if (!app)
        return EFalse;
    TFileName full(app->AppFullName());
    TInt pos = full.LocateReverse('\\');
    if (pos < 0)
        return EFalse;
    aDir = full.Left(pos + 1);
    return ETrue;
    }

void CTgPagerAppUi::LoadSettingsL()
    {
    TFileName dir;
    if (AppDir(dir))
        iSettings->LoadL(iCoeEnv->FsSession(), dir);
    }

void CTgPagerAppUi::SaveSettingsL()
    {
    TFileName dir;
    if (AppDir(dir))
        iSettings->SaveL(iCoeEnv->FsSession(), dir);
    }

void CTgPagerAppUi::ShowStatusL(const TDesC& aText)
    {
    iAppView->AddSysL(aText);
    }

void CTgPagerAppUi::HandleTgEvent(TInt aEvent, const TDesC16* aMsg)
    {
    switch (aEvent)
        {
        case ETgUiMsg:
            if (aMsg)
                {
                TRAPD(err, iAppView->AddMsgL(TGSTR(KStBot), *aMsg));
                if (err == KErrNone)
                    {
                    TRAPD(err2,
                        CAknInformationNote* note = new (ELeave) CAknInformationNote;
                        note->ExecuteLD(*aMsg);
                        );
                    (void) err2;
                    }
                }
            break;
        case ETgUiConnecting:
            iWasConnected = EFalse;
            iDownShown = EFalse;
            TRAPD(e1, ShowStatusL(TGSTR(KStConnecting)));
            (void) e1;
            break;
        case ETgUiUp:
            iWasConnected = ETrue;
            iDownShown = EFalse;
            TRAPD(e2, ShowStatusL(TGSTR(KStConnected)));
            (void) e2;
            break;
        case ETgUiDown:
            {
            TBuf<96> line;
            if (iWasConnected)
                {
                line.Append(TGSTR(KStConnLost));
                line.Append(' ');
                line.Append(TGSTR(KStRetry));
                iWasConnected = EFalse;
                iDownShown = ETrue;
                }
            else if (!iDownShown)
                {
                line.Append(TGSTR(KStNoServer));
                iDownShown = ETrue;
                }
            if (line.Length() > 0)
                {
                TRAPD(e3, ShowStatusL(line));
                (void) e3;
                }
            }
            break;
        case ETgUiAuthErr:
            iWasConnected = EFalse;
            TRAPD(e4, ShowStatusL(TGSTR(KStBadCode)));
            (void) e4;
            break;
        case ETgUiSendErr:
            TRAPD(e5, ShowStatusL(TGSTR(KStNotSent)));
            (void) e5;
            break;
        case ETgUiNoCfg:
            TRAPD(e6, ShowStatusL(TGSTR(KStNeedCfg)));
            (void) e6;
            break;
        default:
            break;
        }
    }

void CTgPagerAppUi::DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenuPane)
    {
    if (aResourceId != R_TGPAGER_MENU)
        return;

    aMenuPane->SetItemTextL(ETgPagerCmdWrite, TGSTR(KStWrite));
    aMenuPane->SetItemTextL(ETgPagerCmdConnect, TGSTR(KStConnect));
    aMenuPane->SetItemTextL(ETgPagerCmdDisconnect, TGSTR(KStDisconnect));
    aMenuPane->SetItemTextL(ETgPagerCmdSettings, TGSTR(KStSettings));
    aMenuPane->SetItemTextL(ETgPagerCmdClear, TGSTR(KStClear));
    aMenuPane->SetItemTextL(EAknSoftkeyExit, TGSTR(KStExit));

    TBool conn = iEngine->Connected();
    aMenuPane->SetItemDimmed(ETgPagerCmdConnect, conn);
    aMenuPane->SetItemDimmed(ETgPagerCmdDisconnect, !conn);
    }

void CTgPagerAppUi::DoWriteDialogL()
    {
    TBuf<200> text;
    CAknTextQueryDialog* dlg = CAknTextQueryDialog::NewL(text);
    CleanupStack::PushL(dlg);
    dlg->SetPromptL(TGSTR(KStPromptMsg));
    CleanupStack::Pop(dlg);
    if (dlg->ExecuteLD(R_TGPAGER_WRITE_QUERY))
        {
        if (text.Length() == 0)
            return;
        iAppView->AddMsgL(TGSTR(KStMe), text);
        TInt err = iEngine->Send(text);
        if (err == KErrTooBig)
            ShowStatusL(TGSTR(KStNotSent));
        }
    }

void CTgPagerAppUi::DoSettingsDialogsL()
    {
    TBuf<64> host(iSettings->iHost);
    TBuf<16> portBuf;
    portBuf.AppendNum(iSettings->iPort);
    TBuf<32> code(iSettings->iCode);

    CAknTextQueryDialog* dlgHost = CAknTextQueryDialog::NewL(host);
    CleanupStack::PushL(dlgHost);
    dlgHost->SetPromptL(TGSTR(KStPromptHost));
    CleanupStack::Pop(dlgHost);
    if (!dlgHost->ExecuteLD(R_TGPAGER_HOST_QUERY))
        return;

    CAknTextQueryDialog* dlgPort = CAknTextQueryDialog::NewL(portBuf);
    CleanupStack::PushL(dlgPort);
    dlgPort->SetPromptL(TGSTR(KStPromptPort));
    CleanupStack::Pop(dlgPort);
    if (!dlgPort->ExecuteLD(R_TGPAGER_PORT_QUERY))
        return;

    TLex lex(portBuf);
    TInt port;
    if (lex.Val(port) != KErrNone || port <= 0 || port > 65535)
        port = 7777;

    CAknTextQueryDialog* dlgCode = CAknTextQueryDialog::NewL(code);
    CleanupStack::PushL(dlgCode);
    dlgCode->SetPromptL(TGSTR(KStPromptCode));
    CleanupStack::Pop(dlgCode);
    if (!dlgCode->ExecuteLD(R_TGPAGER_CODE_QUERY))
        return;

    iSettings->iHost.Copy(host.Left(iSettings->iHost.MaxLength()));
    iSettings->iPort = port;
    iSettings->iCode.Copy(code.Left(iSettings->iCode.MaxLength()));
    SaveSettingsL();

    TBool wasConn = iEngine->Connected();
    iEngine->Disconnect();
    iEngine->Configure(iSettings->iHost, iSettings->iPort, iSettings->iCode);
    ShowStatusL(TGSTR(KStSaved));
    if (wasConn || iSettings->iHost.Length() > 0)
        iEngine->Connect();
    }

void CTgPagerAppUi::HandleCommandL(TInt aCommand)
    {
    switch (aCommand)
        {
        case ETgPagerCmdWrite:
            DoWriteDialogL();
            break;
        case ETgPagerCmdConnect:
            iEngine->Connect();
            break;
        case ETgPagerCmdDisconnect:
            iEngine->Disconnect();
            iWasConnected = EFalse;
            ShowStatusL(TGSTR(KStDisconnected));
            break;
        case ETgPagerCmdSettings:
            DoSettingsDialogsL();
            break;
        case ETgPagerCmdClear:
            iAppView->ClearL();
            break;
        case EEikCmdExit:
        case EAknSoftkeyExit:
            Exit();
            break;
        default:
            Panic(ETgPagerUi);
            break;
        }
    }

// End of File
