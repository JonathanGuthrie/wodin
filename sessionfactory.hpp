#if !defined(_SESSIONFACTORY_HPP_INCLUDED_)
#define _SESSIONFACTORY_HPP_INCLUDED_

#include "internetsession.hpp"

class ServerMaster;
class SessionDriver;


class SessionFactory {
public:
  SessionFactory(void);
  virtual ~SessionFactory(void);
  virtual InternetSession *NewSession(Socket *s, ServerMaster *master, SessionDriver *driver) = 0;
};

#endif //_SESSIONFACTORY_HPP_INCLUDED_
