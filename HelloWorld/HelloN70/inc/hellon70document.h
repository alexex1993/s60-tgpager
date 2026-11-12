/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __HELLON70DOCUMENT_H__
#define __HELLON70DOCUMENT_H__

// INCLUDES
#include <akndoc.h>

// FORWARD DECLARATIONS
class CHelloN70AppUi;
class CEikApplication;


// CLASS DECLARATION

/**
* CHelloN70Document application class.
* An instance of class CHelloN70Document is the Document part of the
* AVKON application framework for the HelloN70 example application.
*/
class CHelloN70Document : public CAknDocument
    {
    public: // Constructors and destructor

        /**
        * NewL.
        * Two-phased constructor.
        * Construct a CHelloN70Document for the AVKON application aApp
        * using two phase construction, and return a pointer
        * to the created object.
        * @param aApp Application creating this document.
        * @return A pointer to the created instance of CHelloN70Document.
        */
        static CHelloN70Document* NewL( CEikApplication& aApp );

        /**
        * NewLC.
        * Two-phased constructor.
        * Construct a CHelloN70Document for the AVKON application aApp
        * using two phase construction, and return a pointer
        * to the created object.
        * @param aApp Application creating this document.
        * @return A pointer to the created instance of CHelloN70Document.
        */
        static CHelloN70Document* NewLC( CEikApplication& aApp );

        /**
        * ~CHelloN70Document
        * Virtual Destructor.
        */
        virtual ~CHelloN70Document();

    public: // Functions from base classes

        /**
        * CreateAppUiL
        * From CEikDocument, CreateAppUiL.
        * Create a CHelloN70AppUi object and return a pointer to it.
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
        * CHelloN70Document.
        * C++ default constructor.
        * @param aApp Application creating this document.
        */
        CHelloN70Document( CEikApplication& aApp );

    };

#endif // __HELLON70DOCUMENT_H__

// End of File