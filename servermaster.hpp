#if !defined(_SERVERMASTER_HPP_INCLUDED_)
#define _SERVERMASTER_HPP_INCLUDED_

#include "sessionfactory.hpp"

class SessionDriver;
class InternetServer;

class ServerMaster {
public:
  ServerMaster(void);
  virtual ~ServerMaster(void) = 0;
  virtual SessionFactory *GetSessionFactory(void) = 0;
  virtual SessionDriver *NewDriver(InternetServer *server, int pipeFd) = 0;
};

#endif //_SERVERMASTER_HPP_INCLUDED_
