#if !defined(_IMAPUSER_HPP_INCLUDED_)
#define _IMAPUSER_HPP_INCLUDED_

// SYZYGY -- I need to know whether HavePlaintextPassword will return true or not
// SYZYGY -- even under circumstances when I can't create a user because I haven't
// SYZYGY -- yet gotten that far.  For example, it alters the capability string if
// SYZYGY -- I have access to the plain text password because I can then do CRAM and
// SYZYGY -- other SASL methods.
// SYZYGY -- The two approaches I've come up with so far are these:  Put a HavePlaintextPassword
// SYZYGY -- method into the server and (possibly) make that method static in the descendents
// SYZYGY -- of ImapUser and create a user when the session is created, and then have a 
// SYZYGY -- SetUsername method that will do the stuff that the constructor does now.
// SYZYGY -- I don't know which I like better.
class ImapUser {
public:
    ImapUser(const char *user);
    virtual ~ImapUser();
    virtual bool HavePlaintextPassword() = 0;
    virtual bool CheckCredentials(const char *password) = 0;
    virtual char *GetPassword(void) const = 0;

protected:
    char *name;
    bool userFound;
};

#endif // _IMAPUSER_HPP_INCLUDED_
