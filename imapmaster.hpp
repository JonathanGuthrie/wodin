#if !defined(_IMAPMASTER_HPP_INCLUDED_)
#define _IMAPMASTER_HPP_INCLUDED_

#include <clotho/servermaster.hpp>

#include "imapsession.hpp"

class SessionDriver;
class ImapUser;
class Namespace;

class ImapMaster : public ServerMaster {
public:
  ImapMaster(std::string fqdn, unsigned login_timeout = 60,
	     unsigned idle_timeout = 1800, unsigned asynchronous_event_time = 900, unsigned bad_login_pause = 5, unsigned max_retries = 12, unsigned retry_seconds = 5);
  virtual ~ImapMaster(void);
  virtual ImapSession *newSession(SessionDriver *driver, Server *server);

  ImapUser *userInfo(const char *userid);
  Namespace *mailStore(ImapSession *session);
  bool isAnonymousEnabled() { return false; } // SYZYGY ANONYMOUS
  // This will send the specified message out the specified socket
  // after the specified number of seconds and then set that session
  // up to receive again
  time_t idleTimeout(void) const { return m_idleTimeout; /* seconds */ }
  time_t loginTimeout(void) const { return m_loginTimeout; /* seconds */ }
  time_t asynchronousEventTime(void) const { return m_asynchronousEventTime; /* seconds */ }
  const std::string &fqdn(void) const { return m_fqdn; }
  bool useConfiguredUid(void) const { return m_useConfiguredUid; }
  uid_t configuredUid(void) const { return m_configuredUid; }
  bool useConfiguredGid(void) const { return m_useConfiguredGid; }
  uid_t configuredGid(void) const { return m_configuredGid; }
  void fqdn(const std::string &fqdn) { m_fqdn = fqdn; }
  unsigned badLoginPause(void) const { return m_badLoginPause; }
  unsigned maxRetries(void) const { return m_maxRetries; }
  unsigned retryDelaySeconds(void) const { return m_retryDelaySeconds; }
#if 0
  void idleTimer(SessionDriver *driver, unsigned seconds);
  void scheduleAsynchronousAction(SessionDriver *driver, unsigned seconds);
  void scheduleRetry(SessionDriver *driver, unsigned seconds);
  void delaySend(SessionDriver *driver, unsigned seconds, const std::string &message);
#endif // 0

private:
  unsigned m_idleTimeout;
  unsigned m_loginTimeout;
  unsigned m_asynchronousEventTime;
  bool m_useConfiguredUid;
  uid_t m_configuredUid;
  bool m_useConfiguredGid;
  gid_t m_configuredGid;
  std::string m_fqdn;
  unsigned m_badLoginPause;
  unsigned m_maxRetries;
  unsigned m_retryDelaySeconds;
};

#endif //_IMAPMASTER_HPP_INCLUDED_
