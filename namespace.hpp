#if !defined(_NAMESPACE_HPP_INCLUDED_)
#define _NAMESPACE_HPP_INCLUDED_

#include <map>

#include "mailstore.hpp"

class Namespace : public MailStore
{
public:
    typedef enum {
	PERSONAL,
	OTHERS,
	SHARED,
	BAD_NAMESPACE
    } NAMESPACE_TYPES;
	
    typedef struct {
	MailStore *store;
	char separator;
	NAMESPACE_TYPES type;
    } namespace_t;

    typedef std::map<std::string, namespace_t> NamespaceMap;

    Namespace(ImapSession *session);
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
    virtual ~Namespace();
    void AddNamespace(NAMESPACE_TYPES type, const std::string &name, MailStore *handler, char separator = '\0');
    const std::string ListNamespaces(void) const;

private:
    MailStore *getNameSpace(const std::string &name);
    NamespaceMap namespaces;
    NAMESPACE_TYPES defaultType;
    MailStore *defaultNamespace;
    MailStore *selectedNamespace;
    char defaultSeparator;
};

#endif // _NAMESPACE_HPP_INCLUDED_
