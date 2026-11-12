/* Copyright (c) 2004, Nokia. All rights reserved */


// INCLUDE FILES
#include "HelloN70Document.h"
#include "HelloN70Application.h"

// ============================ MEMBER FUNCTIONS ===============================

// UID for the application;
// this should correspond to the uid defined in the mmp file
const TUid KUidHelloN70App = { 0x70000070 };

// -----------------------------------------------------------------------------
// CHelloN70Application::CreateDocumentL()
// Creates CApaDocument object
// -----------------------------------------------------------------------------
//
CApaDocument* CHelloN70Application::CreateDocumentL()
    {
    // Create an HelloN70 document, and return a pointer to it
    return (static_cast<CApaDocument*>
                    ( CHelloN70Document::NewL( *this ) ) );
    }

// -----------------------------------------------------------------------------
// CHelloN70Application::AppDllUid()
// Returns application UID
// -----------------------------------------------------------------------------
//
TUid CHelloN70Application::AppDllUid() const
    {
    // Return the UID for the HelloN70 application
    return KUidHelloN70App;
    }

// End of File