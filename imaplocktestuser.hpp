#if !defined(_IMAPLOCKTESTUSER_HPP_INCLUDED_)
#define _IMAPLOCKTESTUSER_HPP_INCLUDED_

#include <string>

#include "imapuser.hpp"

class ImapLockTestUser : ImapUser {
public:
  ImapLockTestUser(const char *user, const ImapMaster *Master);
  virtual ~ImapLockTestUser();
  virtual bool havePlaintextPassword() { return false; } 
  virtual bool checkCredentials(const char *password) { return true; }
  virtual char *password(void) const { return NULL; }
  virtual char *homeDir(void) const { return m_home; }

private:
  char *m_home;
};

#endif // _IMAPLOCKTESTUSER_HPP_INCLUDED_
