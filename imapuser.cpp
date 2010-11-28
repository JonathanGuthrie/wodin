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
