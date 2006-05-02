#include <string.h>

#include "imapuser.hpp"

ImapUser::ImapUser(const char *user)
{
    name = strdup(user);
    userFound = false;
}

ImapUser::~ImapUser()
{
    delete[] name;
}
