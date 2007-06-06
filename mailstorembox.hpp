#if !defined(_MAILSTOREMBOX_HPP_INCLUDED_)
#define _MAILSTOREMBOX_HPP_INCLUDED_

#include <fstream>

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
    virtual MailStore::MAIL_STORE_RESULT AddMessageToMailbox(const std::string &MailboxName, uint8_t *data, size_t length,
						  DateTime &createTime, uint32_t messageFlags, size_t *newUid = NULL);
    virtual MailStore::MAIL_STORE_RESULT AppendDataToMessage(const std::string &MailboxName, size_t uid, uint8_t *data, size_t length);
    virtual MailStore::MAIL_STORE_RESULT DoneAppendingDataToMessage(const std::string &MailboxName, size_t uid);
    virtual unsigned GetSerialNumber();
    virtual unsigned GetUidValidityNumber() { return m_uidValidity; }
    virtual MailStore::MAIL_STORE_RESULT MailboxOpen(const std::string &MailboxName, bool readWrite = true);
    virtual MailStore::MAIL_STORE_RESULT PurgeDeletedMessages(NUMBER_LIST *nowGone);

    virtual MAIL_STORE_RESULT GetMailboxCounts(const std::string &MailboxName, uint32_t which, unsigned &messageCount,
					       unsigned &recentCount, unsigned &uidNext, unsigned &uidValidity, unsigned &firstUnseen);

    virtual unsigned MailboxMessageCount() { return m_mailboxMessageCount; }
    virtual unsigned MailboxRecentCount() { return m_recentCount; }
    virtual unsigned MailboxFirstUnseen() { return m_firstUnseen; }

    // This updates the flags associated with the email message
    // of 'orig' is the original flag set, then the final flag set is 
    // orMask | (andMask & orig)
    // The final flag set is returned in flags
    virtual MailStore::MAIL_STORE_RESULT MessageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags);

    virtual std::string GetMailboxUserPath() const ;
    virtual MailStore::MAIL_STORE_RESULT MailboxFlushBuffers(NUMBER_LIST *nowGone);
    virtual MailStore::MAIL_STORE_RESULT MailboxUpdateStats(NUMBER_LIST *nowGone);
    virtual void BuildMailboxList(const char *ref, const char *pattern, MAILBOX_LIST *result, bool listAll);
    virtual ~MailStoreMbox();
    // This deletes a message in a mail box
    virtual MailStore::MAIL_STORE_RESULT DeleteMessage(const std::string &MailboxName, size_t uid);

private:
    typedef struct {
	unsigned uid;
	uint32_t flags;
	std::fstream::pos_type start;
	unsigned imapLength;
	bool isDirty;
    } MessageIndex_t;

    typedef std::vector<MessageIndex_t> MESSAGE_INDEX;
    MESSAGE_INDEX m_messageIndex;
    const char *m_homeDirectory;
    const char *m_inboxPath;
    unsigned m_mailboxMessageCount;
    unsigned m_recentCount;
    unsigned m_firstUnseen;
    unsigned m_uidValidity;
    unsigned m_uidLast;
    bool m_hasHiddenMessage;
    bool m_hasDeletedMessage;
    bool m_isDirty;
    std::ofstream *m_outFile;
    // Appendstate is used as part of the append process.  It's used to detect any "\n>*From " strings in messages so that I can 
    // properly quote them as the message is imported.  I need to create a state transition diagram, somewhere.
    int m_appendState;
    time_t m_lastMtime; // The mtime of the file the last time it was at a known good state.
    void ListAll(const char *pattern, MAILBOX_LIST *result);
    void ListSubscribed(const char *pattern, MAILBOX_LIST *result);
    bool isMailboxInteresting(const std::string path);
    bool ListAllHelper(const regex_t *compiled_regex, const char *home_directory, const char *working_dir, MAILBOX_LIST *result, int maxdepth);
    bool ParseMessage(std::ifstream &inFile, bool firstMessage, bool &countMessage, unsigned &uidValidity, unsigned &uidNext, MessageIndex_t &messageMetaData);
    void AddDataToMessageFile(uint8_t *data, size_t length);
    MailStore::MAIL_STORE_RESULT FlushAndExpunge(NUMBER_LIST *nowGone, bool doExpunge);
};

#endif // _MAILSTOREMBOX_HPP_INCLUDED_
