/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __TGPAGERDOCUMENT_H__
#define __TGPAGERDOCUMENT_H__

// INCLUDES
#include <akndoc.h>

// FORWARD DECLARATIONS
class CTgPagerAppUi;
class CEikApplication;


// CLASS DECLARATION

/**
* CTgPagerDocument application class.
* An instance of class CTgPagerDocument is the Document part of the
* AVKON application framework for the TgPager example application.
*/
class CTgPagerDocument : public CAknDocument
    {
    public: // Constructors and destructor

        /**
        * NewL.
        * Two-phased constructor.
        * Construct a CTgPagerDocument for the AVKON application aApp
        * using two phase construction, and return a pointer
        * to the created object.
        * @param aApp Application creating this document.
        * @return A pointer to the created instance of CTgPagerDocument.
        */
        static CTgPagerDocument* NewL( CEikApplication& aApp );

        /**
        * NewLC.
        * Two-phased constructor.
        * Construct a CTgPagerDocument for the AVKON application aApp
        * using two phase construction, and return a pointer
        * to the created object.
        * @param aApp Application creating this document.
        * @return A pointer to the created instance of CTgPagerDocument.
        */
        static CTgPagerDocument* NewLC( CEikApplication& aApp );

        /**
        * ~CTgPagerDocument
        * Virtual Destructor.
        */
        virtual ~CTgPagerDocument();

    public: // Functions from base classes

        /**
        * CreateAppUiL
        * From CEikDocument, CreateAppUiL.
        * Create a CTgPagerAppUi object and return a pointer to it.
        * The object returned is owned by the Uikon framework.
        * @return Pointer to created instance of AppUi.
        */
        CEikAppUi* CreateAppUiL();

    private: // Constructors

        /**
        * ConstructL
        * 2nd phase constructor.
        */
        void ConstructL();

        /**
        * CTgPagerDocument.
        * C++ default constructor.
        * @param aApp Application creating this document.
        */
        CTgPagerDocument( CEikApplication& aApp );

    };

#endif // __TGPAGERDOCUMENT_H__

// End of File