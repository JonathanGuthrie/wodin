#if !defined(_IMAPDRIVER_HPP_INCLUDED_)
#define _IMAPDRIVER_HPP_INCLUDED_

#include <pthread.h>

#include <libcppserver/socket.hpp>
#include <libcppserver/sessiondriver.hpp>

class InternetServer;
class ImapSession;
class ServerMaster;

// The SessionDriver class sits between the server, which does the listening
// for more data, and the InternetSession, which does all the processing of the
// data.  It knows server-specific stuff and the merest bit of session stuff.
// This has to be a separate class from InternetServer because I want one of these
// for each InternetSession
class ImapDriver : public SessionDriver
{
public:
  ImapDriver(InternetServer *s, ServerMaster *master);
  virtual ~ImapDriver();
  virtual void DoWork(void);
  virtual void NewSession(Socket *s);
  virtual void DestroySession(void);
  void DoAsynchronousWork(void);
  void DoRetry(void);

private:
  pthread_mutex_t m_workMutex;
};

#endif //_IMAPDRIVER_HPP_INCLUDED_
