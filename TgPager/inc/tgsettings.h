#ifndef __TGSETTINGS_H__
#define __TGSETTINGS_H__

#include <e32base.h>
#include <f32file.h>

class CTgSettings : public CBase
    {
    public:
        static CTgSettings* NewL();
        ~CTgSettings();

        void LoadL(RFs& aFs, const TDesC& aDir);
        void SaveL(RFs& aFs, const TDesC& aDir);

        TBuf<64> iHost;
        TInt iPort;
        TBuf<32> iCode;

    private:
        CTgSettings();
        void ConstructL();
        void DefaultValues();
    };

#endif // __TGSETTINGS_H__
