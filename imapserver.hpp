#if !defined(_IMAPSERVER_HPP_INCLUDED_)
#define _IMAPSERVER_HPP_INCLUDED_

#include "socket.hpp"
#include "imapuser.hpp"
#include "namespace.hpp"
#include "ThreadPool.hpp"

class ImapSession;
class SessionDriver;
class ServerMaster;

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
  ImapServer(uint32_t bind_address, short bind_port, ServerMaster *master) throw(ServerErrorException);
  ~ImapServer();
  void Run();
  void Shutdown();
  void WantsToReceive(int which);
  void KillSession(SessionDriver *driver);

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
  ServerMaster *m_master;
};

#endif // _IMAPSERVER_HPP_INCLUDED_
