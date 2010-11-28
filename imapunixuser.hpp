#if !defined(_IMAPUNIXUSER_HPP_INCLUDED_)
#define _IMAPUNIXUSER_HPP_INCLUDED_

#include <string>

#include "imapuser.hpp"

class ImapUnixUser : ImapUser {
public:
  ImapUnixUser(const char *user, const ImapMaster *Master);
  virtual ~ImapUnixUser();
  virtual bool havePlaintextPassword();
  virtual bool checkCredentials(const char *password);
  virtual char *password(void) const;
  virtual char *homeDir(void) const { return m_home; };

private:
  char *m_home;
};

#endif // _IMAPUNIXUSER_HPP_INCLUDED_
