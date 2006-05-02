#if !defined(_IMAPUNIXUSER_HPP_INCLUDED_)
#define _IMAPUNIXUSER_HPP_INCLUDED_

#include <string>

#include "imapuser.hpp"

class ImapUnixUser : ImapUser {
public:
    ImapUnixUser(const char *user);
    virtual ~ImapUnixUser();
    virtual bool HavePlaintextPassword();
    virtual bool CheckCredentials(const char *password);
    virtual char * GetPassword(void) const;

private:
    uid_t uid;
    gid_t gid;
    char *home;
};

#endif // _IMAPUNIXUSER_HPP_INCLUDED_
