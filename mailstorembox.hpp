#if !defined(_MAILSTOREMBOX_HPP_INCLUDED_)
#define _MAILSTOREMBOX_HPP_INCLUDED_

#include <regex.h>

#include "mailstore.hpp"

class MailStoreMbox : public MailStore
{
public:
    MailStoreMbox(ImapSession *session, const char *usersInboxPath, const char *usersHomeDirectory);
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
    virtual ~MailStoreMbox();

private:
    const char *homeDirectory;
    const char *inboxPath;
    unsigned mailboxMessageCount;
    unsigned recentCount;
    unsigned firstUnseen;
    unsigned uidValidity;
    void ListAll(const char *pattern, MAILBOX_LIST *result);
    void ListSubscribed(const char *pattern, MAILBOX_LIST *result);
    bool isMailboxInteresting(const std::string path);
    bool ListAllHelper(const regex_t *compiled_regex, const char *home_directory, const char *working_dir, MAILBOX_LIST *result, int maxdepth);
};

#endif // _MAILSTOREMBOX_HPP_INCLUDED_
