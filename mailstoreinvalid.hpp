#if !defined(_MAILSTOREINVALID_HPP_INCLUDED_)
#define _MAILSTOREINVALID_HPP_INCLUDED_

#include "mailstore.hpp"

class MailStoreInvalid : public MailStore
{
public:
    typedef enum {
	PERSONAL,
	OTHERS,
	SHARED
    } NAMESPACE_TYPES;
	
    MailStoreInvalid(ImapSession *session);
    virtual MailStore::MAIL_STORE_RESULT CreateMailbox(const std::string &MailboxName);
    virtual MailStore::MAIL_STORE_RESULT DeleteMailbox(const std::string &MailboxName);
    virtual MailStore::MAIL_STORE_RESULT RenameMailbox(const std::string &SourceName, const std::string &DestinationName);
    virtual MailStore::MAIL_STORE_RESULT MailboxClose();
    virtual MailStore::MAIL_STORE_RESULT SubscribeMailbox(const std::string &MailboxName, bool isSubscribe);
    virtual unsigned GetSerialNumber();
    virtual unsigned GetNextSerialNumber();
    virtual unsigned GetUidValidityNumber();
    virtual unsigned MailboxMessageCount(const std::string &MailboxName);
    virtual unsigned MailboxRecentCount(const std::string &MailboxName);
    virtual unsigned MailboxFirstUnseen(const std::string &MailboxName);
    virtual unsigned MailboxMessageCount();
    virtual unsigned MailboxRecentCount();
    virtual unsigned MailboxFirstUnseen();
    virtual std::string GetMailboxUserPath() const ;
    virtual MailStore::MAIL_STORE_RESULT MailboxUpdateStats(NUMBER_LIST *nowGone);
    virtual void BuildMailboxList(const char *ref, const char *pattern, MAILBOX_LIST *result, bool listAll);
    virtual ~MailStoreInvalid();

private:
};

#endif // _MAILSTOREINVALID_HPP_INCLUDED_
