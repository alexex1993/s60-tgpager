#ifndef __TGPAGERAPPUI_H__
#define __TGPAGERAPPUI_H__

#include <aknappui.h>

#include "tgpagerengine.h"

class CTgPagerAppView;
class CTgSettings;

class CTgPagerAppUi : public CAknAppUi, public MTgUiObserver
    {
    public:
        void ConstructL();
        CTgPagerAppUi();
        virtual ~CTgPagerAppUi();

    public:
        void HandleCommandL(TInt aCommand);
        void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenuPane);
        void HandleTgEvent(TInt aEvent, const TDesC16* aMsg);

    private:
        void DoWriteDialogL();
        void DoSettingsDialogsL();
        void ShowStatusL(const TDesC& aText);
        void LoadSettingsL();
        void SaveSettingsL();
        TBool AppDir(TFileName& aDir);

    private:
        CTgPagerAppView* iAppView;
        CTgEngine* iEngine;
        CTgSettings* iSettings;
        TBool iWasConnected;
        TBool iDownShown;
    };

#endif // __TGPAGERAPPUI_H__
