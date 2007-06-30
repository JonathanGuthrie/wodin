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
    virtual MailStore::MAIL_STORE_RESULT AddMessageToMailbox(const std::string &MailboxName, uint8_t *data, size_t length,
						  DateTime &createTime, uint32_t messageFlags, size_t *newUid = NULL);
    virtual MailStore::MAIL_STORE_RESULT AppendDataToMessage(const std::string &MailboxName, size_t uid, uint8_t *data, size_t length);
    virtual MailStore::MAIL_STORE_RESULT DoneAppendingDataToMessage(const std::string &MailboxName, size_t uid);
    virtual unsigned GetSerialNumber();
    virtual unsigned GetUidValidityNumber();
    virtual MailStore::MAIL_STORE_RESULT MailboxOpen(const std::string &MailboxName, bool readWrite = true);
    virtual MailStore::MAIL_STORE_RESULT PurgeDeletedMessages(NUMBER_LIST *nowGone);

    virtual MailStore::MAIL_STORE_RESULT GetMailboxCounts(const std::string &MailboxName, uint32_t which, unsigned &messageCount,
							  unsigned &recentCount, unsigned &uidNext, unsigned &uidValidity,
							  unsigned &firstUnseen);
    virtual unsigned MailboxMessageCount();
    virtual unsigned MailboxRecentCount();
    virtual unsigned MailboxFirstUnseen();

    virtual const DateTime &MessageInternalDate(const unsigned long uid);

    virtual NUMBER_LIST MailboxMsnToUid(const NUMBER_LIST &msns);
    virtual unsigned long MailboxMsnToUid(unsigned long msn);
    virtual NUMBER_LIST MailboxUidToMsn(const NUMBER_LIST &uids);
    virtual unsigned long MailboxUidToMsn(unsigned long uid);

    // This updates the flags associated with the email message
    // of 'orig' is the original flag set, then the final flag set is 
    // orMask | (andMask & orig)
    // The final flag set is returned in flags
    virtual MailStore::MAIL_STORE_RESULT MessageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags);

    virtual std::string GetMailboxUserPath() const ;
    virtual MailStore::MAIL_STORE_RESULT MailboxFlushBuffers(NUMBER_LIST *nowGone);
    virtual MailStore::MAIL_STORE_RESULT MailboxUpdateStats(NUMBER_LIST *nowGone);
    virtual void BuildMailboxList(const std::string &pattern, MAILBOX_LIST *result, bool listAll);
    virtual ~Namespace();
    void AddNamespace(NAMESPACE_TYPES type, const std::string &name, MailStore *handler, char separator = '\0');
    const std::string ListNamespaces(void) const;
    // This deletes a message in a mail box
    virtual MailStore::MAIL_STORE_RESULT DeleteMessage(const std::string &MailboxName, unsigned long uid);
    virtual MailMessage::MAIL_MESSAGE_RESULT GetMessageData(MailMessage **message, unsigned long uid);
    virtual size_t GetBufferLength(unsigned long uid);
    virtual MailStore::MAIL_STORE_RESULT OpenMessageFile(unsigned long uid);
    virtual size_t ReadMessage(char *buff, size_t offset, size_t length);
    virtual void CloseMessageFile(void);
    virtual const SEARCH_RESULT *SearchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate);

private:
    MailStore *getNameSpace(const std::string &name);
    NamespaceMap namespaces;
    NAMESPACE_TYPES defaultType;
    MailStore *defaultNamespace;
    MailStore *selectedNamespace;
    char defaultSeparator;
};

#endif // _NAMESPACE_HPP_INCLUDED_
