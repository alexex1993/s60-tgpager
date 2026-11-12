/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __TGPAGERAPPLICATION_H__
#define __TGPAGERAPPLICATION_H__

// INCLUDES
#include <aknapp.h>


// CLASS DECLARATION

/**
* CTgPagerApplication application class.
* Provides factory to create concrete document object.
* An instance of CTgPagerApplication is the application part of the
* AVKON application framework for the TgPager example application.
*/
class CTgPagerApplication : public CAknApplication
    {
    public: // Functions from base classes

        /**
        * From CApaApplication, AppDllUid.
        * @return Application's UID (KUidTgPagerApp).
        */
        TUid AppDllUid() const;

    protected: // Functions from base classes

        /**
        * From CApaApplication, CreateDocumentL.
        * Creates CTgPagerDocument document object. The returned
        * pointer in not owned by the CTgPagerApplication object.
        * @return A pointer to the created document object.
        */
        CApaDocument* CreateDocumentL();
    };

#endif // __TGPAGERAPPLICATION_H__

// End of File