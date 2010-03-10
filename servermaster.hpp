
#if !defined(_SERVERMASTER_HPP_INCLUDED_)
#define _SERVERMASTER_HPP_INCLUDED_

#include "sessionfactory.hpp"

class SessionDriver;

class ServerMaster {
public:
  ServerMaster(void);
  virtual ~ServerMaster(void) = 0;
  virtual SessionFactory *GetSessionFactory(void) = 0;
};

#endif //_SERVERMASTER_HPP_INCLUDED_
