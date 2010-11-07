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
  virtual ImapSession *NewSession(SessionDriver *driver, InternetServer *server);

  ImapUser *GetUserInfo(const char *userid);
  Namespace *GetMailStore(ImapSession *session);
  bool IsAnonymousEnabled() { return false; } // SYZYGY ANONYMOUS
  // This will send the specified message out the specified socket
  // after the specified number of seconds and then set that session
  // up to receive again
  time_t GetIdleTimeout(void) const { return m_idleTimeout; /* seconds */ }
  time_t GetLoginTimeout(void) const { return m_loginTimeout; /* seconds */ }
  time_t GetAsynchronousEventTime(void) const { return m_asynchronousEventTime; /* seconds */ }
  const std::string &GetFQDN(void) const { return m_fqdn; }
  bool UseConfiguredUid(void) const { return m_useConfiguredUid; }
  uid_t GetConfiguredUid(void) const { return m_configuredUid; }
  bool UseConfiguredGid(void) const { return m_useConfiguredGid; }
  uid_t GetConfiguredGid(void) const { return m_configuredGid; }
  void SetFQDN(const std::string &fqdn) { m_fqdn = fqdn; }
  unsigned GetBadLoginPause(void) const { return m_badLoginPause; }
  unsigned GetMaxRetries(void) const { return m_maxRetries; }
  unsigned GetRetryDelaySeconds(void) const { return m_retryDelaySeconds; }
#if 0
  void SetIdleTimer(SessionDriver *driver, unsigned seconds);
  void ScheduleAsynchronousAction(SessionDriver *driver, unsigned seconds);
  void ScheduleRetry(SessionDriver *driver, unsigned seconds);
  void DelaySend(SessionDriver *driver, unsigned seconds, const std::string &message);
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
