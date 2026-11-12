#include <coecntrl.h>
#include <aknlists.h>
#include <eikclbd.h>
#include <badesca.h>
#include <eiklbx.h>

#include "tgpagerappview.h"

const TInt KTgMaxItems = 120;
const TInt KTgWrapWidth = 26;
const TInt KTgLineMax = 64;

CTgPagerAppView::CTgPagerAppView()
    {
    }

CTgPagerAppView* CTgPagerAppView::NewL(const TRect& aRect)
    {
    CTgPagerAppView* self = NewLC(aRect);
    CleanupStack::Pop(self);
    return self;
    }

CTgPagerAppView* CTgPagerAppView::NewLC(const TRect& aRect)
    {
    CTgPagerAppView* self = new (ELeave) CTgPagerAppView;
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    return self;
    }

void CTgPagerAppView::ConstructL(const TRect& aRect)
    {
    CreateWindowL();
    SetRect(aRect);

    iItems = new (ELeave) CDesCArrayFlat(8);

    iListBox = new (ELeave) CAknSingleStyleListBox;
    iListBox->SetContainerWindowL(*this);
    iListBox->ConstructL(this, 0);
    iListBox->SetExtent(TPoint(0, 0), Size());
    iListBox->Model()->SetItemTextArray(iItems);
    iListBox->Model()->SetOwnershipType(ELbmDoesNotOwnItemArray);
    iListBox->CreateScrollBarFrameL(ETrue);
    iListBox->ScrollBarFrame()->SetScrollBarVisibilityL(
        CEikScrollBarFrame::EOff, CEikScrollBarFrame::EAuto);
    iListBox->SetFocus(ETrue);

    ActivateL();
    }

CTgPagerAppView::~CTgPagerAppView()
    {
    delete iListBox;
    delete iItems;
    }

void CTgPagerAppView::SizeChanged()
    {
    if (iListBox)
        iListBox->SetExtent(TPoint(0, 0), Size());
    }

TInt CTgPagerAppView::CountComponentControls() const
    {
    return 1;
    }

CCoeControl* CTgPagerAppView::ComponentControl(TInt aIndex) const
    {
    if (aIndex == 0)
        return iListBox;
    return NULL;
    }

TKeyResponse CTgPagerAppView::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
    {
    if (iListBox)
        return iListBox->OfferKeyEventL(aKeyEvent, aType);
    return EKeyWasNotConsumed;
    }

void CTgPagerAppView::CurrentTime(TDes& aTime) const
    {
    TTime now;
    now.HomeTime();
    TDateTime dt(now.DateTime());
    aTime.Zero();
    if (dt.Hour() < 10)
        aTime.Append('0');
    aTime.AppendNum(dt.Hour());
    aTime.Append(':');
    if (dt.Minute() < 10)
        aTime.Append('0');
    aTime.AppendNum(dt.Minute());
    }

void CTgPagerAppView::AppendWrappedL(const TDesC& aText)
    {
    // CAknSingleStyleListBox: формат строки "\tтекст" (поле до '\t' —
    // индекс иконки). Без '\t' текст не отображается — пустые строки.
    TBuf<KTgLineMax> line;
    TBool first = ETrue;
    TInt i = 0;
    while (i < aText.Length())
        {
        line.Zero();
        line.Append('\t');
        if (!first)
            {
            line.Append(' ');
            line.Append(' ');
            }
        TInt spaceLeft = KTgWrapWidth - line.Length();
        TInt chunk = aText.Length() - i;
        if (chunk > spaceLeft)
            {
            TInt brk = -1;
            for (TInt k = i + spaceLeft; k > i; k--)
                {
                if (aText[k - 1] == ' ')
                    {
                    brk = k;
                    break;
                    }
                }
            if (brk > i && (brk - i) <= spaceLeft)
                chunk = brk - i;
            else
                chunk = spaceLeft;
            }
        line.Append(aText.Mid(i, chunk));
        i += chunk;
        while (i < aText.Length() && aText[i] == ' ')
            i++;
        iItems->AppendL(line);
        first = EFalse;
        }
    if (first)
        {
        line.Zero();
        line.Append('\t');
        iItems->AppendL(line);
        }
    if (iItems->Count() > KTgMaxItems)
        {
        TInt excess = iItems->Count() - KTgMaxItems;
        iItems->Delete(0, excess);
        iListBox->HandleItemRemovalL();
        }
    iListBox->HandleItemAdditionL();
    iListBox->SetCurrentItemIndexAndDraw(iItems->Count() - 1);
    }

void CTgPagerAppView::AddMsgL(const TDesC& aWho, const TDesC& aText)
    {
    TBuf<640> full;
    TBuf<8> time;
    CurrentTime(time);
    full.Append('[');
    full.Append(time);
    full.Append(']');
    full.Append(' ');
    full.Append(aWho);
    full.Append(':');
    full.Append(' ');
    TInt remain = full.MaxLength() - full.Length();
    if (aText.Length() <= remain)
        full.Append(aText);
    else
        full.Append(aText.Left(remain));
    AppendWrappedL(full);
    }

void CTgPagerAppView::AddSysL(const TDesC& aText)
    {
    TBuf<640> full;
    TBuf<8> time;
    CurrentTime(time);
    full.Append('[');
    full.Append(time);
    full.Append(']');
    full.Append(' ');
    TInt remain = full.MaxLength() - full.Length();
    if (aText.Length() <= remain)
        full.Append(aText);
    else
        full.Append(aText.Left(remain));
    AppendWrappedL(full);
    }

void CTgPagerAppView::ClearL()
    {
    iItems->Reset();
    iListBox->HandleItemRemovalL();
    iListBox->Reset();
    }
