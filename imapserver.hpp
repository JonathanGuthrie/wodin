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
    void DoAsynchronousWork(void);
    void DoRetry(void);
    void NewSession(Socket *s);
    const ImapSession *GetSession(void) const { return m_session; }
    void DestroySession(void);
    Socket *GetSocket(void) const { return m_sock; }
    ImapServer *GetServer(void) const { return m_server; }

private:
    ImapSession *m_session;
    ImapServer *m_server;
    Socket *m_sock;
    int m_pipe;
    pthread_mutex_t m_workMutex;
};


typedef ThreadPool<SessionDriver *> ImapWorkerPool;

class ServerErrorException
{
public:
    ServerErrorException(int error) : m_systemError(error) {}
private:
    int m_systemError;
};

class ImapServer {
public:
    ImapServer(uint32_t bind_address, short bind_port, std::string fqdn, unsigned login_timeout = 60,
	       unsigned idle_timeout = 1800, unsigned asynchronous_event_time = 900, unsigned bad_login_pause = 5) throw(ServerErrorException);
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
    time_t GetIdleTimeout(void) const { return m_idleTimeout; /* seconds */ }
    time_t GetLoginTimeout(void) const { return m_loginTimeout; /* seconds */ }
    time_t GetAsynchronousEventTime(void) const { return m_asynchronousEventTime; /* seconds */ }
    void SetIdleTimer(SessionDriver *driver, unsigned seconds);
    void ScheduleAsynchronousAction(SessionDriver *driver, unsigned seconds);
    void ScheduleRetry(SessionDriver *driver, unsigned seconds);
    void KillSession(SessionDriver *driver);
    const std::string &GetFQDN(void) const { return m_fqdn; }
    bool UseConfiguredUid(void) const { return m_useConfiguredUid; }
    uid_t GetConfiguredUid(void) const { return m_configuredUid; }
    bool UseConfiguredGid(void) const { return m_useConfiguredGid; }
    uid_t GetConfiguredGid(void) const { return m_configuredGid; }
    void SetFQDN(const std::string &fqdn) { m_fqdn = fqdn; }
    unsigned GetBadLoginPause(void) const { return m_badLoginPause; }

private:
    bool m_isRunning;
    Socket *m_listener;
    // static void *SelectThreadFunction(void *);
    static void *ListenerThreadFunction(void *);
    static void *ReceiverThreadFunction(void *);
    static void *TimerQueueFunction(void *);
    
    ImapWorkerPool *m_pool;
    pthread_t m_listenerThread, m_receiverThread, m_timerQueueThread;
    SessionDriver *m_sessions[FD_SETSIZE];
    fd_set m_masterFdList;
    pthread_mutex_t m_masterFdMutex;
    int m_pipeFd[2];
    DeltaQueue m_timerQueue;
    unsigned m_idleTimeout;
    unsigned m_loginTimeout;
    unsigned m_asynchronousEventTime;
    bool m_useConfiguredUid;
    uid_t m_configuredUid;
    bool m_useConfiguredGid;
    gid_t m_configuredGid;
    std::string m_fqdn;
    unsigned m_badLoginPause;
};

#endif // _IMAPSERVER_HPP_INCLUDED_
