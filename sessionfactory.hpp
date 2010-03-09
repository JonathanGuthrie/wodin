#if !defined(_SESSIONFACTORY_HPP_INCLUDED_)
#define _SESSIONFACTORY_HPP_INCLUDED_

#include "imapsession.hpp"

class SessionFactory {
public:
  SessionFactory(void);
  virtual ~SessionFactory(void);
  virtual ImapSession *NewSession(Socket *s, ImapServer *server, SessionDriver *driver) = 0;
};

#endif //_SESSIONFACTORY_HPP_INCLUDED_
