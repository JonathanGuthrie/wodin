#if !defined(_IMAPUNIXUSER_HPP_INCLUDED_)
#define _IMAPUNIXUSER_HPP_INCLUDED_

#include <string>

#include "imapuser.hpp"

class ImapUnixUser : ImapUser {
public:
    ImapUnixUser(const char *user, const ImapServer *server);
    virtual ~ImapUnixUser();
    virtual bool HavePlaintextPassword();
    virtual bool CheckCredentials(const char *password);
    virtual char *GetPassword(void) const;
    virtual char *GetHomeDir(void) const { return home; };

private:
    char *home;
};

#endif // _IMAPUNIXUSER_HPP_INCLUDED_
