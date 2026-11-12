#ifndef __TGPAGERENGINE_H__
#define __TGPAGERENGINE_H__

#include <e32base.h>
#include <es_sock.h>
#include <in_sock.h>

class CTgEngine;

enum TTgUiEvent
    {
    ETgUiMsg,
    ETgUiConnecting,
    ETgUiUp,
    ETgUiDown,
    ETgUiAuthErr,
    ETgUiConnErr,
    ETgUiSendErr,
    ETgUiNoCfg
    };

class MTgUiObserver
    {
    public:
        virtual void HandleTgEvent(TInt aEvent, const TDesC16* aMsg) = 0;
    };

class CTgConn : public CActive
    {
    public:
        virtual ~CTgConn();
        TBool IsBusy() const { return iState != EIdle; }

    protected:
        CTgConn(RSocketServ& aServ);
        TInt Start(const TDesC& aHost, TInt aPort, const TDesC& aCode);
        void Stop();
        TInt IssueWrite(const TDesC8& aPayload);
        TInt IssueRead();
        TInt IssueWait(TInt aMicroSeconds);
        void CloseSocket();

    private:
        void RunL();
        void DoCancel();
        TInt ConnectAddr(TInetAddr& aAddr);
        void StartAuth();
        void Feed(const TDesC8& aData);
        void HandleAuthLine(const TDesC8& aLine);

    protected:
        virtual void OnAuthed() = 0;
        virtual void OnLine(const TDesC8& aLine) = 0;
        virtual void OnWriteDone() = 0;
        virtual void OnWaitDone() = 0;
        virtual void OnError(TInt aErr, TBool aAuthErr) = 0;

    protected:
        enum TConnState
            {
            EIdle,
            EResolve,
            EConnect,
            EAuthW,
            EAuthR,
            EBusyW,
            EBusyR,
            EWait
            };

        RSocketServ& iServ;
        RSocket iSock;
        RHostResolver iResolver;
        TNameEntry iNameEntry;
        RTimer iTimer;
        TConnState iState;
        TBool iSockOpen;
        TBool iTimerCreated;
        TBool iAuthed;
        TBool iSkipLine;
        TBuf<64> iHost;
        TInt iPort;
        TBuf<32> iCode;
        TBuf8<4096> iTx8;
        TBuf8<4096> iLine8;
        TBuf8<1024> iRx8;
        TSockXfrLength iXfr;
    };

class CTgPollConn : public CTgConn
    {
    public:
        CTgPollConn(RSocketServ& aServ, CTgEngine& aEngine);
        TInt Begin(const TDesC& aHost, TInt aPort, const TDesC& aCode);
        void Halt();
        TBool IsUp() const { return iUp; }

    protected:
        void OnAuthed();
        void OnLine(const TDesC8& aLine);
        void OnWriteDone();
        void OnWaitDone();
        void OnError(TInt aErr, TBool aAuthErr);

    private:
        void Poll();

        CTgEngine& iEngine;
        TBool iWant;
        TBool iUp;
        TBool iRetry;
    };

class CTgSendConn : public CTgConn
    {
    public:
        CTgSendConn(RSocketServ& aServ, CTgEngine& aEngine);
        void Setup(const TDesC& aHost, TInt aPort, const TDesC& aCode);
        TInt Send(const TDesC16& aText);

    protected:
        void OnAuthed();
        void OnLine(const TDesC8& aLine);
        void OnWriteDone();
        void OnWaitDone();
        void OnError(TInt aErr, TBool aAuthErr);

    private:
        CTgEngine& iEngine;
        TBuf8<2048> iPayload;
    };

class CTgEngine : public CBase
    {
    public:
        static CTgEngine* NewL(MTgUiObserver& aUi);
        ~CTgEngine();

        void Configure(const TDesC& aHost, TInt aPort, const TDesC& aCode);
        TInt Connect();
        void Disconnect();
        TBool Connected() const;
        TInt Send(const TDesC16& aText);

        void PollUp();
        void PollDown();
        void PollAuthFailed();
        void Message(const TDesC16& aText);
        void SendDone();
        void SendFailed();

    private:
        CTgEngine(MTgUiObserver& aUi);
        void ConstructL();
        void FlushPending();
        void KickPending();

        MTgUiObserver& iUi;
        RSocketServ iServ;
        CTgPollConn* iPoll;
        CTgSendConn* iSend;
        TBuf<64> iHost;
        TInt iPort;
        TBuf<32> iCode;
        TBuf16<200> iPending;
        TBool iPendingValid;
    };

#endif // __TGPAGERENGINE_H__
