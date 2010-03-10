#include "imapunixuser.hpp"
#include "mailstorembox.hpp"
#include "mailstoreinvalid.hpp"
#include "namespace.hpp"
#include "imapmaster.hpp"

ImapMaster::ImapMaster(std::string fqdn, unsigned login_timeout, unsigned idle_timeout, unsigned asynchronous_event_time, unsigned bad_login_pause) {
  m_useConfiguredUid = false;
  m_configuredUid = 0;
  m_useConfiguredGid = false;
  m_configuredGid = 0;
  m_loginTimeout = login_timeout;
  m_idleTimeout = idle_timeout;
  m_asynchronousEventTime = asynchronous_event_time;
  m_badLoginPause = bad_login_pause;
  m_factory = new ImapSessionFactory(bad_login_pause);

  ImapSession::BuildSymbolTables();

}

ImapMaster::~ImapMaster(void) {
}

ImapSessionFactory *ImapMaster::GetSessionFactory(void) {
  return m_factory;
}

void ImapMaster::Tick(void) {
  m_timerQueue.Tick();
}

void ImapMaster::PurgeTimer(SessionDriver *driver) {
  m_timerQueue.PurgeSession(driver);
}


ImapUser *ImapMaster::GetUserInfo(const char *userid) {
  return (ImapUser *) new ImapUnixUser(userid, this);
}

Namespace *ImapMaster::GetMailStore(ImapSession *session) {
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

void ImapMaster::DelaySend(SessionDriver *driver, unsigned seconds, const std::string &message) {
  m_timerQueue.AddSend(driver, seconds, message);
}

void ImapMaster::SetIdleTimer(SessionDriver *driver, unsigned seconds) {
  m_timerQueue.AddTimeout(driver, seconds);
}

void ImapMaster::ScheduleAsynchronousAction(SessionDriver *driver, unsigned seconds) {
  m_timerQueue.AddAsynchronousAction(driver, seconds);
}

void ImapMaster::ScheduleRetry(SessionDriver *driver, unsigned seconds) {
  m_timerQueue.AddRetry(driver, seconds);
}
