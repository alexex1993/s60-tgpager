/* Copyright (c) 2004, Nokia. All rights reserved */


// INCLUDE FILES
#include <coemain.h>
#include "HelloN70AppView.h"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CHelloN70AppView::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CHelloN70AppView* CHelloN70AppView::NewL( const TRect& aRect )
    {
    CHelloN70AppView* self = CHelloN70AppView::NewLC( aRect );
    CleanupStack::Pop( self );
    return self;
    }

// -----------------------------------------------------------------------------
// CHelloN70AppView::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CHelloN70AppView* CHelloN70AppView::NewLC( const TRect& aRect )
    {
    CHelloN70AppView* self = new ( ELeave ) CHelloN70AppView;
    CleanupStack::PushL( self );
    self->ConstructL( aRect );
    return self;
    }

// -----------------------------------------------------------------------------
// CHelloN70AppView::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CHelloN70AppView::ConstructL( const TRect& aRect )
    {
    // Create a window for this application view
    CreateWindowL();

    // Set the windows size
    SetRect( aRect );

    // Activate the window, which makes it ready to be drawn
    ActivateL();
    }

// -----------------------------------------------------------------------------
// CHelloN70AppView::CHelloN70AppView()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CHelloN70AppView::CHelloN70AppView()
    {
    // No implementation required
    }


// -----------------------------------------------------------------------------
// CHelloN70AppView::~CHelloN70AppView()
// Destructor.
// -----------------------------------------------------------------------------
//
CHelloN70AppView::~CHelloN70AppView()
    {
    // No implementation required
    }


// -----------------------------------------------------------------------------
// CHelloN70AppView::Draw()
// Draws the display.
// -----------------------------------------------------------------------------
//
void CHelloN70AppView::Draw( const TRect& /*aRect*/ ) const
    {
    // Get the standard graphics context
    CWindowGc& gc = SystemGc();

    // Gets the control's extent
    TRect rect = Rect();

    // Clears the screen
    gc.Clear( rect );

    _LIT( KHelloText, "Hello Habr 2026!" );

    const CFont* font = iCoeEnv->NormalFont();
    TInt textWidth = font->TextWidthInPixels( KHelloText );
    TInt baseline = rect.Height() / 2 + font->AscentInPixels() / 2;

    gc.UseFont( font );
    gc.SetPenColor( KRgbBlack );
    gc.DrawText( KHelloText, TPoint( ( rect.Width() - textWidth ) / 2, baseline ) );
    gc.DiscardFont();
    }

// End of File