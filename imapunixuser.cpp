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

#include <pwd.h>
#include <shadow.h>
#include <ctype.h>
#include <string.h>

#include "imapmaster.hpp"
#include "imapunixuser.hpp"

ImapUnixUser::ImapUnixUser(const char *user, const ImapMaster *master) : ImapUser(user, master) {
  m_home = NULL;
  struct passwd *pass;
  setpwent();
  while (NULL != (pass = getpwent())) {
    if (0 == strcmp(pass->pw_name, user)) {
      if (master->useConfiguredUid()) {
	m_uid = master->configuredUid();
      }
      else {
	m_uid = pass->pw_uid;
      }
      if (master->useConfiguredGid()) {
	m_gid = master->configuredGid();
      }
      else {
	m_gid = pass->pw_gid;
      }
      m_home = new char[1+strlen(pass->pw_dir)];
      strcpy(m_home, pass->pw_dir);
      endpwent();
      m_userFound = true;
      return;
    }
  }
  endpwent();
  // SYZYGY -- I think this should throw an exception for user not found
}

ImapUnixUser::~ImapUnixUser() {
  if (NULL != m_home) {
    delete[] m_home;
  }
}

bool ImapUnixUser::havePlaintextPassword() const {
  return false;
}

bool ImapUnixUser::checkCredentials(const char *password) const {
  if (m_userFound) {
    struct spwd *shadow =  getspnam(m_name->c_str());
    bool result = false;

    if (NULL != shadow) {
      // std::cout << "The encrypted password is \"" << shadow->sp_pwdp << "\"" << std::endl;
      if (isalnum(shadow->sp_pwdp[0]) || ('$' == shadow->sp_pwdp[0]) ||
	  ('/' == shadow->sp_pwdp[0]) || ('\\' == shadow->sp_pwdp[0]) ||
	  ('.' == shadow->sp_pwdp[0])) {
	char salt[32];
	if ('$' == shadow->sp_pwdp[0]) {
	  char *end;

	  strncpy(salt, shadow->sp_pwdp, 31);
	  salt[31] = '\0';
	  end=strchr(salt+1, '$');
	  if (NULL != end) {
	    end = strchr(end+1, '$');
	    if (NULL != end) {
	      *end = '\0';
	    }
	    else {
	      salt[0] = '\0';
	    }
	  }
	  else {
	    salt[0] = '\0';
	  }
	}
	else {
	  strncpy(salt, shadow->sp_pwdp, 2);
	  salt[2] = '\0';
	}

	if ('\0' != salt[0]) {
	  char *crypt_result = crypt(password, salt);
	  // std::cout << "The calculated password is \"" << crypt_result << "\"" << std::endl;
	  result = (0 == strcmp(crypt_result, shadow->sp_pwdp));
	}
      }
    }
    return result;
  }
  else {
    return false;
  }
}


// I have no plaintext password, so I can't return it.
char *ImapUnixUser::password(void) const {
  return NULL;
}
