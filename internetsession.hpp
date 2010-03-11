#if !defined(_INTERNETSESSION_HPP_INCLUDED_)
#define _INTERNETSESSION_HPP_INCLUDED_

#include "socket.hpp"

class ServerMaster;
class SessionDriver;

class InternetSession {
public:
  InternetSession(Socket *sock, ServerMaster *master, SessionDriver *driver);
  virtual ~InternetSession();

protected:
  Socket *m_s;
  ServerMaster *m_master;
  SessionDriver *m_driver;
};

#endif //_INTERNETSESSION_HPP_INCLUDED_
