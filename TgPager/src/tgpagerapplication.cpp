/* Copyright (c) 2004, Nokia. All rights reserved */


// INCLUDE FILES
#include "TgPagerDocument.h"
#include "TgPagerApplication.h"

// ============================ MEMBER FUNCTIONS ===============================

// UID for the application;
// this should correspond to the uid defined in the mmp file
const TUid KUidTgPagerApp = { 0x70000072 };

// -----------------------------------------------------------------------------
// CTgPagerApplication::CreateDocumentL()
// Creates CApaDocument object
// -----------------------------------------------------------------------------
//
CApaDocument* CTgPagerApplication::CreateDocumentL()
    {
    // Create an TgPager document, and return a pointer to it
    return (static_cast<CApaDocument*>
                    ( CTgPagerDocument::NewL( *this ) ) );
    }

// -----------------------------------------------------------------------------
// CTgPagerApplication::AppDllUid()
// Returns application UID
// -----------------------------------------------------------------------------
//
TUid CTgPagerApplication::AppDllUid() const
    {
    // Return the UID for the TgPager application
    return KUidTgPagerApp;
    }

// End of File