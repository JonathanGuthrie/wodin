#if !defined(_INTERNETSERVER_HPP_INCLUDED_)
#define _INTERNETSERVER_HPP_INCLUDED_

#include "socket.hpp"
#include "deltaqueue.hpp"
#include "ThreadPool.hpp"

class SessionDriver;
class ServerMaster;

typedef ThreadPool<SessionDriver *> WorkerPool;

class ServerErrorException
{
public:
  ServerErrorException(int error) : m_systemError(error) {}
private:
  int m_systemError;
};

class InternetServer {
public:
  InternetServer(uint32_t bind_address, short bind_port, ServerMaster *master, int num_worker_threads = 10) throw(ServerErrorException);
  ~InternetServer();
  void Run();
  void Shutdown();
  void WantsToReceive(int which);
  void KillSession(SessionDriver *driver);
  void AddTimerAction(DeltaQueueAction *action);

private:
  bool m_isRunning;
  Socket *m_listener;
  // static void *SelectThreadFunction(void *);
  static void *ListenerThreadFunction(void *);
  static void *ReceiverThreadFunction(void *);
  static void *TimerQueueFunction(void *);
    
  WorkerPool *m_pool;
  pthread_t m_listenerThread, m_receiverThread, m_timerQueueThread;
  SessionDriver *m_sessions[FD_SETSIZE];
  fd_set m_masterFdList;
  pthread_mutex_t m_masterFdMutex;
  int m_pipeFd[2];
  ServerMaster *m_master;
  DeltaQueue m_timerQueue;
  int m_workerCount;
};

#endif // _INTERNETSERVER_HPP_INCLUDED_
