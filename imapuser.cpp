#include <string.h>

#include "imapuser.hpp"

ImapUser::ImapUser(const char *user)
{
    name = new std::string(user);
    userFound = false;
}

ImapUser::~ImapUser()
{
    delete name;
}
