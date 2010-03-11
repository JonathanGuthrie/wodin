#include "imapsessionfactory.hpp"
#include "imapmaster.hpp"

ImapSessionFactory::ImapSessionFactory(unsigned failedLoginPause, unsigned maxRetries, unsigned retrySeconds) : m_failedLoginPause(failedLoginPause), m_maxRetries(maxRetries), m_retrySeconds(retrySeconds) {
}

ImapSessionFactory::~ImapSessionFactory(void) {
}

InternetSession *ImapSessionFactory::NewSession(Socket *s, ServerMaster *master, SessionDriver *driver) {
  ImapMaster *imap_master = dynamic_cast <ImapMaster *>(master);
  return new ImapSession(s, imap_master, driver, m_failedLoginPause, m_maxRetries, m_retrySeconds);
}
