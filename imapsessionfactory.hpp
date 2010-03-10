#if !defined(_IMAPSESSIONFACTORY_HPP_INCLUDED_)
#define _IMAPSESSIONFACTORY_HPP_INCLUDED_

#include "sessionfactory.hpp"
// #include "imapsession.hpp"
class ImapMaster;

class ImapSessionFactory : public SessionFactory {
public:
  ImapSessionFactory(unsigned failedLoginPause = 5, unsigned maxRetries = 12, unsigned retrySeconds = 5);
  virtual ~ImapSessionFactory(void);
  virtual ImapSession *NewSession(Socket *s, ImapMaster *master, SessionDriver *driver);

private:
  unsigned m_failedLoginPause;
  unsigned m_maxRetries;
  unsigned m_retrySeconds;
};

#endif //_IMAPSESSIONFACTORY_HPP_INCLUDED_
