/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __HELLON70APPUI_H__
#define __HELLON70APPUI_H__

// INCLUDES
#include <aknappui.h>


// FORWARD DECLARATIONS
class CHelloN70AppView;


// CLASS DECLARATION
/**
* CHelloN70AppUi application UI class.
* Interacts with the user through the UI and request message processing
* from the handler class
*/
class CHelloN70AppUi : public CAknAppUi
    {
    public: // Constructors and destructor

        /**
        * ConstructL.
        * 2nd phase constructor.
        */
        void ConstructL();

        /**
        * CHelloN70AppUi.
        * C++ default constructor. This needs to be public due to
        * the way the framework constructs the AppUi
        */
        CHelloN70AppUi();

        /**
        * ~CHelloN70AppUi.
        * Virtual Destructor.
        */
        virtual ~CHelloN70AppUi();

    public:  // Functions from base classes

        /**
        * From CEikAppUi, HandleCommandL.
        * Takes care of command handling.
        * @param aCommand Command to be handled.
        */
        void HandleCommandL( TInt aCommand );

    private: // Data

        /**
        * The application view
        * Owned by CHelloN70AppUi
        */
        CHelloN70AppView* iAppView;
    };

#endif // __HELLON70APPUI_H__

// End of File