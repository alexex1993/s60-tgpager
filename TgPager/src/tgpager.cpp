/* Copyright (c) 2004, Nokia. All rights reserved */


// INCLUDE FILES
#include "TgPagerApplication.h"

// -----------------------------------------------------------------------------
// E32Dll()
// Entry point function for Symbian Apps.
// -----------------------------------------------------------------------------
//
GLDEF_C TInt E32Dll( TDllReason /*aReason*/ )
    {
    // DLL entry point, return that everything is ok
    return KErrNone;
    }

// ========================== OTHER EXPORTED FUNCTIONS =========================

// -----------------------------------------------------------------------------
// NewApplication()
// Constructs CTgPagerApplication.
// -----------------------------------------------------------------------------
//
// Create an application, and return a pointer to it
EXPORT_C CApaApplication* NewApplication()
    {
    return ( static_cast<CApaApplication*>( new CTgPagerApplication ) );
    }

// End of File