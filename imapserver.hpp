#if !defined(_IMAPSERVER_HPP_INCLUDED_)
#define _IMAPSERVER_HPP_INCLUDED_

#include "socket.hpp"
#include "imapuser.hpp"
#include "mailstore.hpp"
#include "ThreadPool.hpp"

class SessionDriver;
class ImapSession;
typedef ThreadPool<SessionDriver *> ImapWorkerPool;

class DeltaQueueAction
{
public:
    DeltaQueueAction(int delta);
    class DeltaQueueAction *next;
    virtual void HandleTimeout(bool isPurge) = 0;
    unsigned delta;
};


// SYZYGY -- the idle timer shouldn't be reset.  Instead, I should keep the time the last command
// SYZYGY -- was executed and then check for the timeout period elapsing when the timer expires
// SYZYGY -- and set the timeout for the time since that command happened.
class DeltaQueueIdleTimer : DeltaQueueAction
{
public:
    DeltaQueueIdleTimer(int delta, SessionDriver *driver);
    virtual void HandleTimeout(bool isPurge);

private:
    SessionDriver *driver;
};


class DeltaQueueDelayedMessage : DeltaQueueAction
{
public:
    DeltaQueueDelayedMessage(int delta, SessionDriver *driver, const std::string message); // Note:  Calling copy constructor on the message
    virtual void HandleTimeout(bool isPurge);

private:
    SessionDriver *driver;
    const std::string message;
};


class DeltaQueue
{
public:
    void Tick(void);
    void AddSend(SessionDriver *driver, unsigned seconds, const std::string &message);
    void AddTimeout(SessionDriver *driver, time_t timeout);
    DeltaQueue();
    void InsertNewAction(DeltaQueueAction *action);

private:
    pthread_mutex_t queueMutex;
    DeltaQueueAction *queueHead;
};


class ImapServer {
public:
    ImapServer(uint32_t bind_address, short bind_port, unsigned login_timeout = 60, unsigned idle_timeout = 1800);
    ~ImapServer();
    void Run();
    void Shutdown();
    void WantsToReceive(int which);
    ImapUser *GetUserInfo(const char *userid);
    MailStore *GetMailStore(const ImapUser *user);
    bool IsAnonymousEnabled() { return false; } // SYZYGY ANONYMOUS
    // This will send the specified message out the specified socket
    // after the specified number of seconds and then set that session
    // up to receive again
    void DelaySend(SessionDriver *driver, unsigned seconds, const std::string &message);
    time_t GetIdleTimeout(void) const { return idleTimeout; /* seconds */ }
    time_t GetLoginTimeout(void) const { return loginTimeout; /* seconds */ }
    void SetIdleTimer(SessionDriver *driver, unsigned seconds);
    void KillSession(SessionDriver *driver);

private:
    bool isRunning;
    Socket *listener;
    // static void *SelectThreadFunction(void *);
    static void *ListenerThreadFunction(void *);
    static void *ReceiverThreadFunction(void *);
    static void *TimerQueueFunction(void *);
    
    ImapWorkerPool *pool;
    pthread_t listenerThread, receiverThread, timerQueueThread;
    SessionDriver *sessions[FD_SETSIZE];
    fd_set masterFdList;
    pthread_mutex_t masterFdMutex;
    int pipeFd[2];
    class DeltaQueue timerQueue;
    unsigned idleTimeout;
    unsigned loginTimeout;
};

#endif // _IMAPSERVER_HPP_INCLUDED_
