#if !defined(_IMAPSERVER_HPP_INCLUDED_)
#define _IMAPSERVER_HPP_INCLUDED_

#include "socket.hpp"
#include "imapuser.hpp"
#include "mailstore.hpp"
#include "ThreadPool.hpp"

class SessionDriver;
typedef ThreadPool<SessionDriver *> ImapWorkerPool;

class ImapServer {
public:
    ImapServer(uint32_t bind_address, short bind_port);
    ~ImapServer();
    void Run();
    void Shutdown();
    void WantsToReceive(int which);
    ImapUser *GetUserInfo(const char *userid);
    MailStore *GetMailStore(void);
    bool IsAnonymousEnabled() { return false; } // SYZYGY ANONYMOUS

private:
    bool isRunning;
    Socket *listener;
    // static void *SelectThreadFunction(void *);
    static void *ListenerThreadFunction(void *);
    static void *ReceiverThreadFunction(void *);
    ImapWorkerPool *pool;
    pthread_t listenerThread, receiverThread;
    SessionDriver *sessions[FD_SETSIZE];
    fd_set masterFdList;
    pthread_mutex_t masterFdMutex;
    int pipeFd[2];
};

#endif // _IMAPSERVER_HPP_INCLUDED_
