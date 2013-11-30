/*
 * Copyright 2013 Jonathan R. Guthrie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
