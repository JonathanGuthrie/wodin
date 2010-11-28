#include <clotho/internetserver.hpp>

#include "imaplocktestuser.hpp"
#include "mailstorelocktest.hpp"
#include "namespace.hpp"
#include "imapmaster.hpp"

ImapMaster::ImapMaster(std::string fqdn, unsigned login_timeout, unsigned idle_timeout, unsigned asynchronous_event_time, unsigned bad_login_pause, unsigned max_retries, unsigned retry_seconds) {
  m_useConfiguredUid = false;
  m_configuredUid = 0;
  m_useConfiguredGid = false;
  m_configuredGid = 0;
  m_loginTimeout = login_timeout;
  m_idleTimeout = idle_timeout;
  m_asynchronousEventTime = asynchronous_event_time;
  m_badLoginPause = bad_login_pause;
  m_maxRetries = max_retries;
  m_retryDelaySeconds = retry_seconds;

  ImapSession::buildSymbolTables();
}

ImapMaster::~ImapMaster(void) {
}

ImapUser *ImapMaster::userInfo(const char *userid) {
  return (ImapUser *) new ImapLockTestUser(userid, this);
}

Namespace *ImapMaster::mailStore(ImapSession *session) {
  Namespace *result = new Namespace(session);
  result->addNamespace(Namespace::PERSONAL, "", (MailStore *) new MailStoreLockTest(session), '/');

  return result;
}


ImapSession *ImapMaster::newSession(SessionDriver *driver, Server *server) {
  return new ImapSession(this, driver, server);
}

