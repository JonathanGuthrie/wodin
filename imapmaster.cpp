#include <clotho/internetserver.hpp>

#include "imapunixuser.hpp"
#include "mailstorembox.hpp"
#include "mailstoreinvalid.hpp"
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

  ImapSession::BuildSymbolTables();

}

ImapMaster::~ImapMaster(void) {
}

ImapUser *ImapMaster::userInfo(const char *userid) {
  return (ImapUser *) new ImapUnixUser(userid, this);
}

Namespace *ImapMaster::mailStore(ImapSession *session) {
  Namespace *result = new Namespace(session);
  // need namespaces for #mhinbox, #mh, ~, #shared, #ftp, #news, and #public, just like
  // uw-imap, although all of them have stubbed-out mail stores for them

  result->AddNamespace(Namespace::PERSONAL, "", (MailStore *) new MailStoreMbox(session), '/');
  result->AddNamespace(Namespace::PERSONAL, "#mhinbox", (MailStore *) new MailStoreInvalid(session));
  result->AddNamespace(Namespace::PERSONAL, "#mh/", (MailStore *) new MailStoreInvalid(session), '/');
  result->AddNamespace(Namespace::OTHERS, "~", (MailStore *) new MailStoreInvalid(session), '/');
  result->AddNamespace(Namespace::SHARED, "#shared/", (MailStore *) new MailStoreInvalid(session), '/');
  result->AddNamespace(Namespace::SHARED, "#ftp/", (MailStore *) new MailStoreInvalid(session), '/');
  result->AddNamespace(Namespace::SHARED, "#news.", (MailStore *) new MailStoreInvalid(session), '.');
  result->AddNamespace(Namespace::SHARED, "#public/", (MailStore *) new MailStoreInvalid(session), '/');

  return result;
}


ImapSession *ImapMaster::NewSession(SessionDriver *driver, InternetServer *server) {
  return new ImapSession(this, driver, server);
}

