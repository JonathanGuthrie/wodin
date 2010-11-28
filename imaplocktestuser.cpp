#include "imaplocktestuser.hpp"

ImapLockTestUser::ImapLockTestUser(const char *user, const ImapMaster *Master) : ImapUser(user, Master) {
  m_home = (char *)"/tmp";
}

ImapLockTestUser::~ImapLockTestUser() {
  delete m_home;
}
