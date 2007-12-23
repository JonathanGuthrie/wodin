#if !defined(_NAMESPACE_HPP_INCLUDED_)
#define _NAMESPACE_HPP_INCLUDED_

#include <map>
#include <list>

#include <pthread.h>

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
    virtual MailStore::MAIL_STORE_RESULT ListDeletedMessages(NUMBER_LIST *uidsToBeExpunged);
    virtual MailStore::MAIL_STORE_RESULT ExpungeThisUid(unsigned long uid);

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
    virtual MailStore::MAIL_STORE_RESULT MailboxFlushBuffers(void);
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
    virtual const std::string GenerateUrl(const std::string MailboxName) const;
    virtual Namespace *clone(void);

    static void runtime_init(void) {
	pthread_mutex_init(&m_mailboxMapMutex, NULL);
	m_mailboxMap.clear();
    }

private:
    // SYZYGY -- each item in the MailboxMap class must have a mail store that is talking to the
    // SYZYGY -- mail store for that mailbox, a ref count so that it knows when to clean up, and
    // SYZYGY -- a list of messages that are in the process of being expunged.  Each message
    // SYZYGY -- consists of a message's UID, a count of how many sessions have expunged that
    // SYZYGY -- message, and a list of all the sessions that have expunged it.
    typedef std::list<ImapSession *> SessionList;
    typedef struct {
	unsigned long uid;
	unsigned expungedSessionCount;
	SessionList expungedSessions;
    } expunged_message_t;
    typedef std::map<unsigned, expunged_message_t> ExpungedMessageMap;
    typedef struct {
	MailStore *store;
	ExpungedMessageMap messages;
	int refcount;
	pthread_mutex_t mutex;
    } mailbox_t;
    typedef std::map<std::string, mailbox_t> MailboxMap;
    static MailboxMap m_mailboxMap;
    static pthread_mutex_t m_mailboxMapMutex;

    mailbox_t *m_selectedMailbox;

    MailStore *getNameSpace(std::string &name);
    NamespaceMap namespaces;
    NAMESPACE_TYPES defaultType;
    MailStore *m_defaultNamespace;
    char defaultSeparator;
    void removeUid(unsigned long uid);
    bool addSession(int refCount, expunged_message_t &message);
    unsigned m_mailboxMessageCount;
    void dump_message_session_info(const char *s, ExpungedMessageMap &m);
};

#endif // _NAMESPACE_HPP_INCLUDED_
