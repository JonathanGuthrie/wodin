#if !defined(_IMAPSERVER_HPP_INCLUDED_)
#define _IMAPSERVER_HPP_INCLUDED_

#include "socket.hpp"
#include "imapuser.hpp"
#include "namespace.hpp"
#include "ThreadPool.hpp"
#include "deltaqueue.hpp"

class ImapSession;
class ImapServer;

// The SessionDriver class sits between the server, which does the listening
// for more data, and the ImapSession, which does all the processing of the
// data.  It knows server-specific stuff and the merest bit of session stuff.
// This has to be a separate class from ImapServer because I want one of these
// for each ImapSession
class SessionDriver
{
public:
    SessionDriver(ImapServer *s, int pipe);
    ~SessionDriver();
    void DoWork(void);
    void NewSession(Socket *s);
    const ImapSession *GetSession(void) const { return session; }
    void DestroySession(void);
    Socket *GetSocket(void) const { return sock; }
    ImapServer *GetServer(void) const { return server; }

private:
    ImapSession *session;
    ImapServer *server;
    Socket *sock;
    int pipe;
};


typedef ThreadPool<SessionDriver *> ImapWorkerPool;

class ImapServer {
public:
    ImapServer(uint32_t bind_address, short bind_port, unsigned login_timeout = 60, unsigned idle_timeout = 1800);
    ~ImapServer();
    void Run();
    void Shutdown();
    void WantsToReceive(int which);
    ImapUser *GetUserInfo(const char *userid);
    Namespace *GetMailStore(ImapSession *session);
    bool IsAnonymousEnabled() { return false; } // SYZYGY ANONYMOUS
    // This will send the specified message out the specified socket
    // after the specified number of seconds and then set that session
    // up to receive again
    void DelaySend(SessionDriver *driver, unsigned seconds, const std::string &message);
    time_t GetIdleTimeout(void) const { return idleTimeout; /* seconds */ }
    time_t GetLoginTimeout(void) const { return loginTimeout; /* seconds */ }
    void SetIdleTimer(SessionDriver *driver, unsigned seconds);
    void KillSession(SessionDriver *driver);
    const std::string GetFQDN(void);

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
    DeltaQueue timerQueue;
    unsigned idleTimeout;
    unsigned loginTimeout;
};

#endif // _IMAPSERVER_HPP_INCLUDED_
