#include <string.h>

#include "imapuser.hpp"

ImapUser::ImapUser(const char *user, const ImapServer *server)
{
    name = new std::string(user);
    userFound = false;
}

ImapUser::~ImapUser()
{
    delete name;
}


std::string *ImapUser::ExpandPath(const std::string &specifier) const
{
    std::string *result = new std::string;
    std::string::size_type i, previous;

    for (i = specifier.find_first_of('%', 0), previous = 0; i != std::string::npos; i = specifier.find_first_of('%', i+1)) {
	result->append(specifier.substr(previous, i-previous));
	if (i < specifier.size()-1) {
	    ++i;
	    switch(specifier[i]) {
	    case 'n':
		++i;
		result->append(this->GetName());
		break;

	    case 'h':
		++i;
		result->append(this->GetHomeDir());
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
