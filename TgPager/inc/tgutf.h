#ifndef __TGUTF_H__
#define __TGUTF_H__

#include <e32def.h>

const TUint KTgReplacementChar = 0xFFFD;

class TgUtf
    {
    public:
        // Decode proxy protocol payload (escaped UTF-8) into UTF-16 text.
        // Escapes: "\\" -> '\', "\n" -> ' ' (display). Returns EFalse if truncated.
        static TBool Decode(const TDesC8& aSrc, TDes16& aDst)
            {
            aDst.Zero();
            TInt i = 0;
            TBool esc = EFalse;
            while (i < aSrc.Length())
                {
                TUint8 c = aSrc[i];
                if (c < 0x80)
                    {
                    i++;
                    TText16 ch;
                    if (esc)
                        {
                        esc = EFalse;
                        if (c == 'n')
                            ch = ' ';
                        else if (c == '\\')
                            ch = '\\';
                        else
                            ch = c;
                        }
                    else if (c == '\\')
                        {
                        esc = ETrue;
                        continue;
                        }
                    else
                        {
                        ch = c;
                        }
                    if (aDst.Length() + 1 > aDst.MaxLength())
                        return EFalse;
                    aDst.Append(ch);
                    }
                else
                    {
                    TUint32 cp;
                    TInt len;
                    if ((c & 0xE0) == 0xC0)
                        { len = 2; cp = c & 0x1F; }
                    else if ((c & 0xF0) == 0xE0)
                        { len = 3; cp = c & 0x0F; }
                    else if ((c & 0xF8) == 0xF0)
                        { len = 4; cp = c & 0x07; }
                    else
                        { len = 1; cp = KTgReplacementChar; }
                    i++;
                    TInt k = 1;
                    while (k < len && i < aSrc.Length())
                        {
                        if ((aSrc[i] & 0xC0) != 0x80)
                            break;
                        cp = (cp << 6) | (aSrc[i] & 0x3F);
                        i++;
                        k++;
                        }
                    if (k < len)
                        cp = KTgReplacementChar;
                    if (cp >= 0xD800 && cp <= 0xDFFF)
                        cp = KTgReplacementChar;
                    if (cp <= 0xFFFF)
                        {
                        if (aDst.Length() + 1 > aDst.MaxLength())
                            return EFalse;
                        aDst.Append((TText16)cp);
                        }
                    else
                        {
                        if (aDst.Length() + 2 > aDst.MaxLength())
                            return EFalse;
                        cp -= 0x10000;
                        aDst.Append((TText16)(0xD800 + (cp >> 10)));
                        aDst.Append((TText16)(0xDC00 + (cp & 0x3FF)));
                        }
                    }
                }
            return ETrue;
            }

        // Encode UTF-16 text into escaped UTF-8 protocol payload.
        // Escapes: '\' -> "\\", LF -> "\n". Returns EFalse if truncated.
        static TBool Encode(const TDesC16& aSrc, TDes8& aDst)
            {
            aDst.Zero();
            for (TInt i = 0; i < aSrc.Length(); i++)
                {
                TText16 ch = aSrc[i];
                if (ch == '\\')
                    {
                    if (aDst.Length() + 2 > aDst.MaxLength())
                        return EFalse;
                    aDst.Append('\\');
                    aDst.Append('\\');
                    }
                else if (ch == '\n' || ch == '\r')
                    {
                    if (aDst.Length() + 2 > aDst.MaxLength())
                        return EFalse;
                    aDst.Append('\\');
                    aDst.Append('n');
                    }
                else if (ch < 0x80)
                    {
                    if (aDst.Length() + 1 > aDst.MaxLength())
                        return EFalse;
                    aDst.Append((TUint8)ch);
                    }
                else if (ch < 0x800)
                    {
                    if (aDst.Length() + 2 > aDst.MaxLength())
                        return EFalse;
                    aDst.Append((TUint8)(0xC0 | (ch >> 6)));
                    aDst.Append((TUint8)(0x80 | (ch & 0x3F)));
                    }
                else if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < aSrc.Length()
                         && aSrc[i+1] >= 0xDC00 && aSrc[i+1] <= 0xDFFF)
                    {
                    if (aDst.Length() + 4 > aDst.MaxLength())
                        return EFalse;
                    TUint32 cp = 0x10000
                                 + (((TUint32)(ch - 0xD800)) << 10)
                                 + (aSrc[i+1] - 0xDC00);
                    i++;
                    aDst.Append((TUint8)(0xF0 | (cp >> 18)));
                    aDst.Append((TUint8)(0x80 | ((cp >> 12) & 0x3F)));
                    aDst.Append((TUint8)(0x80 | ((cp >> 6) & 0x3F)));
                    aDst.Append((TUint8)(0x80 | (cp & 0x3F)));
                    }
                else
                    {
                    if (aDst.Length() + 3 > aDst.MaxLength())
                        return EFalse;
                    aDst.Append((TUint8)(0xE0 | (ch >> 12)));
                    aDst.Append((TUint8)(0x80 | ((ch >> 6) & 0x3F)));
                    aDst.Append((TUint8)(0x80 | (ch & 0x3F)));
                    }
                }
            return ETrue;
            }
    };

#endif // __TGUTF_H__
