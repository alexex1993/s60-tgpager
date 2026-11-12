/* Copyright (c) 2004, Nokia. All rights reserved */


// INCLUDE FILES
#include "HelloN70AppUi.h"
#include "HelloN70Document.h"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CHelloN70Document::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CHelloN70Document* CHelloN70Document::NewL( CEikApplication&
                                                          aApp )
    {
    CHelloN70Document* self = NewLC( aApp );
    CleanupStack::Pop( self );
    return self;
    }

// -----------------------------------------------------------------------------
// CHelloN70Document::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CHelloN70Document* CHelloN70Document::NewLC( CEikApplication&
                                                           aApp )
    {
    CHelloN70Document* self =
        new ( ELeave ) CHelloN70Document( aApp );

    CleanupStack::PushL( self );
    self->ConstructL();
    return self;
    }

// -----------------------------------------------------------------------------
// CHelloN70Document::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CHelloN70Document::ConstructL()
    {
    // No implementation required
    }

// -----------------------------------------------------------------------------
// CHelloN70Document::CHelloN70Document()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CHelloN70Document::CHelloN70Document( CEikApplication& aApp )
    : CAknDocument( aApp )
    {
    // No implementation required
    }

// ---------------------------------------------------------------------------
// CHelloN70Document::~CHelloN70Document()
// Destructor.
// ---------------------------------------------------------------------------
//
CHelloN70Document::~CHelloN70Document()
    {
    // No implementation required
    }

// ---------------------------------------------------------------------------
// CHelloN70Document::CreateAppUiL()
// Constructs CreateAppUi.
// ---------------------------------------------------------------------------
//
CEikAppUi* CHelloN70Document::CreateAppUiL()
    {
    // Create the application user interface, and return a pointer to it;
    // the framework takes ownership of this object
    return ( static_cast <CEikAppUi*> ( new ( ELeave )
                                        CHelloN70AppUi ) );
    }

// End of File