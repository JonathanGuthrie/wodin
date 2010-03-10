#if !defined(_SESSIONDRIVER_HPP_INCLUDED_)
#define _SESSIONDRIVER_HPP_INCLUDED_

#include <pthread.h>

#include "socket.hpp"
#include "imapsession.hpp"

class ImapServer;
class ImapSession;
class ServerMaster;

// The SessionDriver class sits between the server, which does the listening
// for more data, and the ImapSession, which does all the processing of the
// data.  It knows server-specific stuff and the merest bit of session stuff.
// This has to be a separate class from ImapServer because I want one of these
// for each ImapSession
class SessionDriver
{
public:
  SessionDriver(ImapServer *s, int pipe, ServerMaster *master);
  ~SessionDriver();
  void DoWork(void);
  void DoAsynchronousWork(void);
  void DoRetry(void);
  void NewSession(Socket *s);
  const ImapSession *GetSession(void) const { return m_session; }
  void DestroySession(void);
  Socket *GetSocket(void) const { return m_sock; }
  ImapServer *GetServer(void) const { return m_server; }
  ServerMaster *GetMaster(void) const { return m_master; }

private:
  ServerMaster *m_master;
  ImapSession *m_session;
  ImapServer *m_server;
  Socket *m_sock;
  int m_pipe;
  pthread_mutex_t m_workMutex;
};

#endif //_SESSIONDRIVER_HPP_INCLUDED_
