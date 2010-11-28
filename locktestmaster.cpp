#include "locktestmaster.hpp"

LockTestMaster::LockTestMaster(std::string fqdn,
			       unsigned login_timeout, 
			       unsigned idle_timeout,
			       unsigned asynchronous_event_time,
			       unsigned bad_login_pause,
			       unsigned max_retries,
			       unsigned retry_seconds) : ImapMaster(fqdn, login_timeout, idle_timeout, asynchronous_event_time, bad_login_pause, max_retries, retry_seconds) {
  m_commands = NULL;
  m_responses = NULL;
}

LockTestMaster::~LockTestMaster() {}

void LockTestMaster::reset(void) {
  delete m_commands;
  delete m_responses;
  m_commands = NULL;
  m_responses = NULL;
}

void LockTestMaster::commands(const Svector &set) {
  m_commands = new Svector(set);
}

const LockTestMaster::Svector &LockTestMaster::commands(void) const {
  return *m_commands;
}

const LockTestMaster::Svector &LockTestMaster::responses(void) const {
  return *m_responses;
}
