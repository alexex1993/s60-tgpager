/* Copyright (c) 2004, Nokia. All rights reserved */


// INCLUDE FILES
#include "TgPagerAppUi.h"
#include "TgPagerDocument.h"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CTgPagerDocument::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CTgPagerDocument* CTgPagerDocument::NewL( CEikApplication&
                                                          aApp )
    {
    CTgPagerDocument* self = NewLC( aApp );
    CleanupStack::Pop( self );
    return self;
    }

// -----------------------------------------------------------------------------
// CTgPagerDocument::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CTgPagerDocument* CTgPagerDocument::NewLC( CEikApplication&
                                                           aApp )
    {
    CTgPagerDocument* self =
        new ( ELeave ) CTgPagerDocument( aApp );

    CleanupStack::PushL( self );
    self->ConstructL();
    return self;
    }

// -----------------------------------------------------------------------------
// CTgPagerDocument::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CTgPagerDocument::ConstructL()
    {
    // No implementation required
    }

// -----------------------------------------------------------------------------
// CTgPagerDocument::CTgPagerDocument()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CTgPagerDocument::CTgPagerDocument( CEikApplication& aApp )
    : CAknDocument( aApp )
    {
    // No implementation required
    }

// ---------------------------------------------------------------------------
// CTgPagerDocument::~CTgPagerDocument()
// Destructor.
// ---------------------------------------------------------------------------
//
CTgPagerDocument::~CTgPagerDocument()
    {
    // No implementation required
    }

// ---------------------------------------------------------------------------
// CTgPagerDocument::CreateAppUiL()
// Constructs CreateAppUi.
// ---------------------------------------------------------------------------
//
CEikAppUi* CTgPagerDocument::CreateAppUiL()
    {
    // Create the application user interface, and return a pointer to it;
    // the framework takes ownership of this object
    return ( static_cast <CEikAppUi*> ( new ( ELeave )
                                        CTgPagerAppUi ) );
    }

// End of File