/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __HELLON70APPLICATION_H__
#define __HELLON70APPLICATION_H__

// INCLUDES
#include <aknapp.h>


// CLASS DECLARATION

/**
* CHelloN70Application application class.
* Provides factory to create concrete document object.
* An instance of CHelloN70Application is the application part of the
* AVKON application framework for the HelloN70 example application.
*/
class CHelloN70Application : public CAknApplication
    {
    public: // Functions from base classes

        /**
        * From CApaApplication, AppDllUid.
        * @return Application's UID (KUidHelloN70App).
        */
        TUid AppDllUid() const;

    protected: // Functions from base classes

        /**
        * From CApaApplication, CreateDocumentL.
        * Creates CHelloN70Document document object. The returned
        * pointer in not owned by the CHelloN70Application object.
        * @return A pointer to the created document object.
        */
        CApaDocument* CreateDocumentL();
    };

#endif // __HELLON70APPLICATION_H__

// End of File