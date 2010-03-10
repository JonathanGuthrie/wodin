#include "imapsessionfactory.hpp"
#include "imapmaster.hpp"

ImapSessionFactory::ImapSessionFactory(unsigned failedLoginPause, unsigned maxRetries, unsigned retrySeconds) : m_failedLoginPause(failedLoginPause), m_maxRetries(maxRetries), m_retrySeconds(retrySeconds) {
}

ImapSessionFactory::~ImapSessionFactory(void) {
}

ImapSession *ImapSessionFactory::NewSession(Socket *s, ImapMaster *master, SessionDriver *driver) {
  return new ImapSession(s, master, driver, m_failedLoginPause, m_maxRetries, m_retrySeconds);
}
