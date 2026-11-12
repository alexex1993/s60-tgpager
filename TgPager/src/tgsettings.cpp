#include <f32file.h>
#include <bautils.h>

#include "tgsettings.h"

_LIT(KTgCfgFile, "\\tgp.cfg");

CTgSettings::CTgSettings()
    {
    DefaultValues();
    }

void CTgSettings::DefaultValues()
    {
    iHost.Zero();
    iPort = 7777;
    iCode.Zero();
    }

CTgSettings* CTgSettings::NewL()
    {
    CTgSettings* self = new (ELeave) CTgSettings;
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }

void CTgSettings::ConstructL()
    {
    }

CTgSettings::~CTgSettings()
    {
    }

void CTgSettings::LoadL(RFs& aFs, const TDesC& aDir)
    {
    DefaultValues();
    TParse parse;
    User::LeaveIfError(parse.Set(KTgCfgFile, &aDir, NULL));
    TFileName path(parse.FullName());
    if (!BaflUtils::FileExists(aFs, path))
        return;

    RFile file;
    User::LeaveIfError(file.Open(aFs, path, EFileRead));
    CleanupClosePushL(file);

    TBuf8<256> buf8;
    User::LeaveIfError(file.Read(buf8));
    CleanupStack::PopAndDestroy(); // file

    TInt pos = 0;
    for (TInt line = 0; line < 3; line++)
        {
        TInt start = pos;
        while (pos < buf8.Length() && buf8[pos] != '\n')
            pos++;
        TInt len = pos - start;
        if (len > 0 && buf8[start + len - 1] == '\r')
            len--;
        TPtrC8 row(buf8.Mid(start, len));
        if (line == 0)
            {
            TBuf8<64> host8;
            TInt n = row.Length();
            if (n > host8.MaxLength())
                n = host8.MaxLength();
            host8.Copy(row.Left(n));
            iHost.Copy(host8);
            }
        else if (line == 1)
            {
            TLex8 lex(row);
            TInt p;
            if (lex.Val(p) == KErrNone && p > 0 && p <= 65535)
                iPort = p;
            }
        else
            {
            TBuf8<32> code8;
            TInt n2 = row.Length();
            if (n2 > code8.MaxLength())
                n2 = code8.MaxLength();
            code8.Copy(row.Left(n2));
            iCode.Copy(code8);
            }
        if (pos < buf8.Length())
            pos++;
        }
    }

void CTgSettings::SaveL(RFs& aFs, const TDesC& aDir)
    {
    TParse parse;
    User::LeaveIfError(parse.Set(KTgCfgFile, &aDir, NULL));
    TFileName path(parse.FullName());

    TBuf8<256> buf8;
    TBuf8<64> tmp;
    tmp.Copy(iHost);
    buf8.Append(tmp);
    buf8.Append('\n');
    buf8.AppendNum(iPort);
    buf8.Append('\n');
    tmp.Copy(iCode);
    buf8.Append(tmp);
    buf8.Append('\n');

    RFile file;
    User::LeaveIfError(file.Replace(aFs, path, EFileWrite));
    CleanupClosePushL(file);
    User::LeaveIfError(file.Write(buf8));
    CleanupStack::PopAndDestroy(); // file
    }
