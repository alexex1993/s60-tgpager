/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __HELLON70APPVIEW_H__
#define __HELLON70APPVIEW_H__

// INCLUDES
#include <coecntrl.h>

// CLASS DECLARATION
class CHelloN70AppView : public CCoeControl
    {
    public: // New methods

        /**
        * NewL.
        * Two-phased constructor.
        * Create a CHelloN70AppView object, which will draw itself to aRect.
        * @param aRect The rectangle this view will be drawn to.
        * @return a pointer to the created instance of CHelloN70AppView.
        */
        static CHelloN70AppView* NewL( const TRect& aRect );

        /**
        * NewLC.
        * Two-phased constructor.
        * Create a CHelloN70AppView object, which will draw itself
        * to aRect.
        * @param aRect Rectangle this view will be drawn to.
        * @return A pointer to the created instance of CHelloN70AppView.
        */
        static CHelloN70AppView* NewLC( const TRect& aRect );

        /**
        * ~CHelloN70AppView
        * Virtual Destructor.
        */
        virtual ~CHelloN70AppView();

    public:  // Functions from base classes

        /**
        * From CCoeControl, Draw
        * Draw this CHelloN70AppView to the screen.
        * @param aRect the rectangle of this view that needs updating
        */
        void Draw( const TRect& aRect ) const;

    private: // Constructors

        /**
        * ConstructL
        * 2nd phase constructor.
        * Perform the second phase construction of a
        * CHelloN70AppView object.
        * @param aRect The rectangle this view will be drawn to.
        */
        void ConstructL(const TRect& aRect);

        /**
        * CHelloN70AppView.
        * C++ default constructor.
        */
        CHelloN70AppView();

    };

#endif // __HELLON70APPVIEW_H__

// End of File