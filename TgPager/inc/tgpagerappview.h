#ifndef __TGPAGERAPPVIEW_H__
#define __TGPAGERAPPVIEW_H__

#include <coecntrl.h>
#include <badesca.h>

class CAknSingleStyleListBox;

class CTgPagerAppView : public CCoeControl
    {
    public:
        static CTgPagerAppView* NewL(const TRect& aRect);
        static CTgPagerAppView* NewLC(const TRect& aRect);
        virtual ~CTgPagerAppView();

        void AddMsgL(const TDesC& aWho, const TDesC& aText);
        void AddSysL(const TDesC& aText);
        void ClearL();

    public:
        TInt CountComponentControls() const;
        CCoeControl* ComponentControl(TInt aIndex) const;
        TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);

    private:
        CTgPagerAppView();
        void ConstructL(const TRect& aRect);
        void SizeChanged();
        void AppendWrappedL(const TDesC& aText);
        void CurrentTime(TDes& aTime) const;

    private:
        CAknSingleStyleListBox* iListBox;
        CDesCArrayFlat* iItems;
    };

#endif // __TGPAGERAPPVIEW_H__
