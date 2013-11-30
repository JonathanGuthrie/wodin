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

#include <string.h>

#include "imapuser.hpp"

ImapUser::ImapUser(const char *user, const ImapMaster *master) {
  m_name = new std::string(user);
  m_userFound = false;
}


ImapUser::~ImapUser() {
  delete m_name;
}


std::string *ImapUser::expandPath(const std::string &specifier) const {
  std::string *result = new std::string;
  std::string::size_type i, previous;

  for (i = specifier.find_first_of('%', 0), previous = 0; i != std::string::npos; i = specifier.find_first_of('%', i+1)) {
    result->append(specifier.substr(previous, i-previous));
    if (i < specifier.size()-1) {
      ++i;
      switch(specifier[i]) {
      case 'n':
	++i;
	result->append(this->name());
	break;

      case 'h':
	++i;
	result->append(this->homeDir());
	break;

      case '%':
	++i;
	result->append("%");
	break;

      default:
	result->append("%");
	break;
      }
    }
    previous = i;
  }
  result->append(specifier.substr(previous));

  return result;
}
