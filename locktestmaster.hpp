#if !defined(_LOCKTESTMASTER_HPP_INCLUDED_)
#define _LOCKTESTMASTER_HPP_INCLUDED_

#include "imapmaster.hpp"

#include <vector>

class LockTestMaster : public ImapMaster {
public:
  typedef std::vector<std::string> Svector;

  LockTestMaster(std::string fqdn, unsigned login_timeout = 60,
	     unsigned idle_timeout = 1800, unsigned asynchronous_event_time = 900, unsigned bad_login_pause = 5, unsigned max_retries = 12, unsigned retry_seconds = 5);
  virtual ~LockTestMaster(void);
  void reset(void);
  const Svector &commands(void) const;
  void commands(const Svector &set);
  const Svector &responses(void) const;
  void response(const std::string &response);

private:
  Svector *m_commands;
  Svector *m_responses;
};

#endif //_LOCKTESTMASTER_HPP_INCLUDED_
