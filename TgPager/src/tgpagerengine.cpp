#include <e32base.h>
#include <es_sock.h>
#include <in_sock.h>

#include "tgutf.h"
#include "tgpagerengine.h"

CTgConn::CTgConn(RSocketServ& aServ)
: CActive(EPriorityStandard), iServ(aServ)
    {
    iTimerCreated = (iTimer.CreateLocal() == KErrNone);
    iState = EIdle;
    iSockOpen = EFalse;
    iAuthed = EFalse;
    iSkipLine = EFalse;
    CActiveScheduler::Add(this);
    }

CTgConn::~CTgConn()
    {
    Cancel();
    if (iSockOpen)
        {
        iSock.Close();
        iSockOpen = EFalse;
        }
    if (iTimerCreated)
        iTimer.Close();
    }

TInt CTgConn::Start(const TDesC& aHost, TInt aPort, const TDesC& aCode)
    {
    if (iState != EIdle || IsActive())
        return KErrInUse;
    if (aHost.Length() == 0 || aPort <= 0 || aPort > 65535)
        return KErrArgument;
    iHost.Copy(aHost.Left(aHost.Length() > iHost.MaxLength() ? iHost.MaxLength() : aHost.Length()));
    iPort = aPort;
    iCode.Copy(aCode.Left(aCode.Length() > iCode.MaxLength() ? iCode.MaxLength() : aCode.Length()));
    iAuthed = EFalse;
    iSkipLine = EFalse;
    iLine8.Zero();

    TInetAddr addr;
    if (addr.Input(iHost) == KErrNone)
        {
        addr.SetPort(iPort);
        return ConnectAddr(addr);
        }

    TInt err = iResolver.Open(iServ, KAfInet, KProtocolInetUdp);
    if (err != KErrNone)
        return err;
    iState = EResolve;
    iResolver.GetByName(iHost, iNameEntry, iStatus);
    SetActive();
    return KErrNone;
    }

TInt CTgConn::ConnectAddr(TInetAddr& aAddr)
    {
    TInt err = iSock.Open(iServ, KAfInet, KSockStream, KProtocolInetTcp);
    if (err != KErrNone)
        return err;
    iSockOpen = ETrue;
    iState = EConnect;
    iSock.Connect(aAddr, iStatus);
    SetActive();
    return KErrNone;
    }

void CTgConn::StartAuth()
    {
    iTx8.Zero();
    iTx8.Append(_L8("AUTH "));
    iTx8.Append(iCode);
    iTx8.Append(_L8("\n"));
    iState = EAuthW;
    iSock.Write(iTx8, iStatus);
    SetActive();
    }

void CTgConn::Stop()
    {
    Cancel();
    CloseSocket();
    iAuthed = EFalse;
    iSkipLine = EFalse;
    iLine8.Zero();
    iState = EIdle;
    }

void CTgConn::CloseSocket()
    {
    if (iSockOpen)
        {
        iSock.CancelAll();
        iSock.Close();
        iSockOpen = EFalse;
        }
    }

TInt CTgConn::IssueWrite(const TDesC8& aPayload)
    {
    if (IsActive())
        return KErrInUse;
    if (aPayload.Length() + 1 > iTx8.MaxLength())
        return KErrTooBig;
    iTx8.Copy(aPayload);
    iTx8.Append('\n');
    iState = EBusyW;
    iSock.Write(iTx8, iStatus);
    SetActive();
    return KErrNone;
    }

TInt CTgConn::IssueRead()
    {
    if (IsActive())
        return KErrInUse;
    iState = EBusyR;
    iSock.RecvOneOrMore(iRx8, 0, iStatus, iXfr);
    SetActive();
    return KErrNone;
    }

TInt CTgConn::IssueWait(TInt aMicroSeconds)
    {
    if (IsActive() || !iTimerCreated)
        return KErrInUse;
    iState = EWait;
    iTimer.After(iStatus, aMicroSeconds);
    SetActive();
    return KErrNone;
    }

void CTgConn::RunL()
    {
    TInt err = iStatus.Int();
    switch (iState)
        {
        case EResolve:
            iResolver.Close();
            if (err != KErrNone)
                {
                iState = EIdle;
                OnError(err, EFalse);
                return;
                }
            {
            TInetAddr addr(TInetAddr::Cast(iNameEntry().iAddr));
            addr.SetPort(iPort);
            TInt rc = ConnectAddr(addr);
            if (rc != KErrNone)
                {
                iState = EIdle;
                OnError(rc, EFalse);
                }
            }
            break;

        case EConnect:
            if (err != KErrNone)
                {
                CloseSocket();
                iState = EIdle;
                OnError(err, EFalse);
                return;
                }
            StartAuth();
            break;

        case EAuthW:
            if (err != KErrNone)
                {
                CloseSocket();
                iState = EIdle;
                OnError(err, EFalse);
                return;
                }
            IssueRead();
            break;

        case EAuthR:
        case EBusyR:
            if (err != KErrNone)
                {
                CloseSocket();
                iState = EIdle;
                OnError(err, EFalse);
                return;
                }
            iState = EBusyR;
            Feed(iRx8);
            if (iState == EAuthR || iState == EBusyR)
                IssueRead();
            break;

        case EBusyW:
            if (err != KErrNone)
                {
                CloseSocket();
                iState = EIdle;
                OnError(err, EFalse);
                return;
                }
            OnWriteDone();
            break;

        case EWait:
            iState = EIdle;
            OnWaitDone();
            break;

        default:
            break;
        }
    }

void CTgConn::Feed(const TDesC8& aData)
    {
    for (TInt i = 0; i < aData.Length(); i++)
        {
        TUint8 c = aData[i];
        if (c == '\n')
            {
            TBool skip = iSkipLine;
            iSkipLine = EFalse;
            if (skip)
                continue;
            TInt len = iLine8.Length();
            if (len > 0 && iLine8[len - 1] == '\r')
                iLine8.SetLength(len - 1);
            TPtrC8 line(iLine8);
            iLine8.Zero();
            if (iAuthed)
                OnLine(line);
            else
                HandleAuthLine(line);
            if (iState == EIdle)
                return;
            }
        else if (!iSkipLine)
            {
            if (iLine8.Length() >= iLine8.MaxLength())
                iSkipLine = ETrue;
            else
                iLine8.Append(c);
            }
        }
    }

void CTgConn::HandleAuthLine(const TDesC8& aLine)
    {
    if (aLine.CompareF(_L8("OK")) == 0)
        {
        iAuthed = ETrue;
        OnAuthed();
        }
    else
        {
        CloseSocket();
        iState = EIdle;
        OnError(KErrPermissionDenied, ETrue);
        }
    }

void CTgConn::DoCancel()
    {
    switch (iState)
        {
        case EResolve:
            iResolver.Cancel();
            iResolver.Close();
            break;
        case EWait:
            iTimer.Cancel();
            break;
        default:
            CloseSocket();
            break;
        }
    iState = EIdle;
    }

CTgPollConn::CTgPollConn(RSocketServ& aServ, CTgEngine& aEngine)
: CTgConn(aServ), iEngine(aEngine)
    {
    iWant = EFalse;
    iUp = EFalse;
    iRetry = EFalse;
    }

TInt CTgPollConn::Begin(const TDesC& aHost, TInt aPort, const TDesC& aCode)
    {
    if (iState != EIdle || IsActive())
        {
        Halt();
        }
    TInt err = Start(aHost, aPort, aCode);
    if (err == KErrNone)
        {
        iWant = ETrue;
        iUp = EFalse;
        iRetry = EFalse;
        }
    return err;
    }

void CTgPollConn::Halt()
    {
    iWant = EFalse;
    iUp = EFalse;
    Stop();
    }

void CTgPollConn::Poll()
    {
    if (IssueWrite(_L8("POLL")) == KErrTooBig)
        {
        CloseSocket();
        iState = EIdle;
        OnError(KErrTooBig, EFalse);
        }
    }

void CTgPollConn::OnAuthed()
    {
    iUp = ETrue;
    iEngine.PollUp();
    Poll();
    }

void CTgPollConn::OnLine(const TDesC8& aLine)
    {
    _LIT8(KMsg, "MSG ");
    if (aLine.Find(KMsg) == 0 && aLine.Length() > 4)
        {
        TBuf16<600> text;
        TgUtf::Decode(aLine.Mid(4), text);
        iEngine.Message(text);
        return;
        }
    if (aLine.CompareF(_L8("END")) == 0)
        {
        Poll();
        return;
        }
    if (aLine.Find(_L8("ERR")) == 0)
        {
        if (IssueWait(3000000) != KErrNone)
            Poll();
        }
    }

void CTgPollConn::OnWriteDone()
    {
    IssueRead();
    }

void CTgPollConn::OnWaitDone()
    {
    if (iRetry)
        {
        iRetry = EFalse;
        if (Start(iHost, iPort, iCode) != KErrNone)
            {
            iWant = EFalse;
            iEngine.PollDown();
            }
        }
    else
        {
        Poll();
        }
    }

void CTgPollConn::OnError(TInt /*aErr*/, TBool aAuthErr)
    {
    iUp = EFalse;
    if (aAuthErr)
        {
        iWant = EFalse;
        iEngine.PollAuthFailed();
        return;
        }
    if (!iWant)
        return;
    iRetry = ETrue;
    iEngine.PollDown();
    IssueWait(10000000);
    }

CTgSendConn::CTgSendConn(RSocketServ& aServ, CTgEngine& aEngine)
: CTgConn(aServ), iEngine(aEngine)
    {
    }

void CTgSendConn::Setup(const TDesC& aHost, TInt aPort, const TDesC& aCode)
    {
    iHost.Copy(aHost.Left(aHost.Length() > iHost.MaxLength() ? iHost.MaxLength() : aHost.Length()));
    iPort = aPort;
    iCode.Copy(aCode.Left(aCode.Length() > iCode.MaxLength() ? iCode.MaxLength() : aCode.Length()));
    }

TInt CTgSendConn::Send(const TDesC16& aText)
    {
    if (iState != EIdle || IsActive())
        return KErrInUse;
    if (iHost.Length() == 0)
        return KErrArgument;
    iPayload.Zero();
    iPayload.Append(_L8("SEND "));
    TBuf8<2048> enc;
    if (!TgUtf::Encode(aText, enc))
        return KErrTooBig;
    if (iPayload.Length() + enc.Length() > iPayload.MaxLength())
        return KErrTooBig;
    iPayload.Append(enc);
    return Start(iHost, iPort, iCode);
    }

void CTgSendConn::OnAuthed()
    {
    if (IssueWrite(iPayload) != KErrNone)
        {
        CloseSocket();
        iState = EIdle;
        iEngine.SendFailed();
        }
    }

void CTgSendConn::OnLine(const TDesC8& aLine)
    {
    TBool ok = (aLine.Find(_L8("OK")) == 0);
    CloseSocket();
    iState = EIdle;
    if (ok)
        iEngine.SendDone();
    else
        iEngine.SendFailed();
    }

void CTgSendConn::OnWriteDone()
    {
    IssueRead();
    }

void CTgSendConn::OnWaitDone()
    {
    CloseSocket();
    iState = EIdle;
    }

void CTgSendConn::OnError(TInt /*aErr*/, TBool /*aAuthErr*/)
    {
    iEngine.SendFailed();
    }

CTgEngine::CTgEngine(MTgUiObserver& aUi)
: iUi(aUi), iPort(7777)
    {
    iPendingValid = EFalse;
    }

void CTgEngine::ConstructL()
    {
    User::LeaveIfError(iServ.Connect());
    iPoll = new (ELeave) CTgPollConn(iServ, *this);
    iSend = new (ELeave) CTgSendConn(iServ, *this);
    }

CTgEngine* CTgEngine::NewL(MTgUiObserver& aUi)
    {
    CTgEngine* self = new (ELeave) CTgEngine(aUi);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }

CTgEngine::~CTgEngine()
    {
    delete iPoll;
    delete iSend;
    iServ.Close();
    }

void CTgEngine::Configure(const TDesC& aHost, TInt aPort, const TDesC& aCode)
    {
    iHost.Copy(aHost.Left(aHost.Length() > iHost.MaxLength() ? iHost.MaxLength() : aHost.Length()));
    iPort = aPort;
    iCode.Copy(aCode.Left(aCode.Length() > iCode.MaxLength() ? iCode.MaxLength() : aCode.Length()));
    iSend->Setup(iHost, iPort, iCode);
    }

TInt CTgEngine::Connect()
    {
    if (iHost.Length() == 0)
        {
        iUi.HandleTgEvent(ETgUiNoCfg, NULL);
        return KErrArgument;
        }
    iUi.HandleTgEvent(ETgUiConnecting, NULL);
    return iPoll->Begin(iHost, iPort, iCode);
    }

void CTgEngine::Disconnect()
    {
    iPoll->Halt();
    }

TBool CTgEngine::Connected() const
    {
    return iPoll->IsUp();
    }

TInt CTgEngine::Send(const TDesC16& aText)
    {
    if (!Connected())
        {
        iUi.HandleTgEvent(ETgUiSendErr, NULL);
        return KErrNotReady;
        }
    TInt err = iSend->Send(aText);
    if (err == KErrInUse)
        {
        TInt n = aText.Length();
        if (n > iPending.MaxLength())
            n = iPending.MaxLength();
        iPending.Copy(aText.Left(n));
        iPendingValid = ETrue;
        return KErrNone;
        }
    return err;
    }

void CTgEngine::PollUp()
    {
    iUi.HandleTgEvent(ETgUiUp, NULL);
    FlushPending();
    }

void CTgEngine::PollDown()
    {
    iUi.HandleTgEvent(ETgUiDown, NULL);
    }

void CTgEngine::PollAuthFailed()
    {
    iUi.HandleTgEvent(ETgUiAuthErr, NULL);
    }

void CTgEngine::Message(const TDesC16& aText)
    {
    iUi.HandleTgEvent(ETgUiMsg, &aText);
    }

void CTgEngine::SendDone()
    {
    FlushPending();
    }

void CTgEngine::SendFailed()
    {
    iUi.HandleTgEvent(ETgUiSendErr, NULL);
    FlushPending();
    }

void CTgEngine::FlushPending()
    {
    if (!iPendingValid || iSend->IsBusy())
        return;
    iPendingValid = EFalse;
    iSend->Send(iPending);
    iPending.Zero();
    }
