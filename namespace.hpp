#if !defined(_NAMESPACE_HPP_INCLUDED_)
#define _NAMESPACE_HPP_INCLUDED_

#include <regex.h>

#include "mailstore.hpp"

class Namespace : public MailStore
{
public:
    Namespace(ImapSession *session);
    virtual MailStore::MAIL_STORE_RESULT CreateMailbox(const std::string &MailboxName);
    virtual MailStore::MAIL_STORE_RESULT DeleteMailbox(const std::string &MailboxName);
    virtual MailStore::MAIL_STORE_RESULT RenameMailbox(const std::string &SourceName, const std::string &DestinationName);
    virtual MailStore::MAIL_STORE_RESULT MailboxClose();
    virtual MailStore::MAIL_STORE_RESULT SubscribeMailbox(const std::string &MailboxName, bool isSubscribe);
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
    virtual ~Namespace();
    void AddNamespace(const std::string &name, Mailstore *handler);
    void ListNameSpaces(void);

private:
};

#endif // _NAMESPACE_HPP_INCLUDED_
