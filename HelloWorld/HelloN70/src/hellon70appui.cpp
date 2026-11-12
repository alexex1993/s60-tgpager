/* Copyright (c) 2004, Nokia. All rights reserved */

// INCLUDE FILES
#include <avkon.hrh>
#include <aknnotewrappers.h>
#include <stringloader.h>
#include <HelloN70.rsg>

#include "HelloN70.pan"
#include "HelloN70AppUi.h"
#include "HelloN70AppView.h"
#include "HelloN70.hrh"

// ============================ MEMBER FUNCTIONS ===============================


// -----------------------------------------------------------------------------
// CHelloN70AppUi::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CHelloN70AppUi::ConstructL()
    {
    // Initialise app UI with standard value.
    BaseConstructL();

    // Create view object
    iAppView = CHelloN70AppView::NewL( ClientRect() );
    }

// -----------------------------------------------------------------------------
// CHelloN70AppUi::CHelloN70AppUi()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CHelloN70AppUi::CHelloN70AppUi()
    {
    // No implementation required
    }

// -----------------------------------------------------------------------------
// CHelloN70AppUi::~CHelloN70AppUi()
// Destructor.
// -----------------------------------------------------------------------------
//
CHelloN70AppUi::~CHelloN70AppUi()
    {
    if ( iAppView )
        {
        delete iAppView;
        iAppView = NULL;
        }
    }

// -----------------------------------------------------------------------------
// CHelloN70AppUi::HandleCommandL()
// Takes care of command handling.
// -----------------------------------------------------------------------------
//
void CHelloN70AppUi::HandleCommandL( TInt aCommand )
    {
    switch( aCommand )
        {
        case EEikCmdExit:
        case EAknSoftkeyExit:
            Exit();
            break;

        case EHelloN70Command1:
            {
            // Load a string from the resource file and display it
            HBufC* textResource = StringLoader::LoadLC( R_HEWB_COMMAND1_TEXT );
            CAknInformationNote* informationNote;

            informationNote = new ( ELeave ) CAknInformationNote;

            // Show the information Note with
            // textResource loaded with StringLoader.
            informationNote->ExecuteLD( *textResource);

            // Pop HBuf from CleanUpStack and Destroy it.
            CleanupStack::PopAndDestroy( textResource );
            }
            break;

        default:
            Panic( EHelloN70Ui );
            break;
        }
    }

// End of File