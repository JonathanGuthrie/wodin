#if !defined(_MAILSTOREMBOX_HPP_INCLUDED_)
#define _MAILSTOREMBOX_HPP_INCLUDED_

#include "mailstore.hpp"

class MailStoreMbox : public MailStore
{
public:
    MailStoreMbox(const char *usersHomeDirectory);
    virtual MailStore::MAIL_STORE_RESULT CreateMailbox(const std::string &MailboxName);
    virtual MailStore::MAIL_STORE_RESULT MailboxClose();
    virtual unsigned GetSerialNumber();
    virtual unsigned GetNextSerialNumber();
    virtual unsigned GetUidValidityNumber() { return uidValidity; }
    virtual unsigned MailboxMessageCount(const std::string &MailboxName);
    virtual unsigned MailboxRecentCount(const std::string &MailboxName);
    virtual unsigned MailboxFirstUnseen(const std::string &MailboxName);
    virtual unsigned MailboxMessageCount() { return mailboxMessageCount; }
    virtual unsigned MailboxRecentCount() { return recentCount; }
    virtual unsigned MailboxFirstUnseen() { return firstUnseen; }
    virtual std::string GetMailboxUserPath() const ;
    virtual MailStore::MAIL_STORE_RESULT MailboxUpdateStats(NUMBER_LIST *nowGone);
    virtual void BuildMailboxList(const char *ref, const char *pattern, MAILBOX_LIST *result, bool listAll);
    virtual ~MailStoreMbox();

private:
    const char *homeDirectory;
    unsigned mailboxMessageCount;
    unsigned recentCount;
    unsigned firstUnseen;
    unsigned uidValidity;
    void ListAll(const char *pattern, MAILBOX_LIST *result);
    bool isInboxInteresting(void);
};

#endif // _MAILSTOREMBOX_HPP_INCLUDED_
