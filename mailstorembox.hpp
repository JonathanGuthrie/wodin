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

    virtual MAIL_STORE_RESULT GetMailboxCounts(const std::string &MailboxName, uint32_t which, unsigned &messageCount,
					       unsigned &recentCount, unsigned &uidNext, unsigned &uidValidity, unsigned &firstUnseen);

    virtual unsigned MailboxMessageCount() { return m_mailboxMessageCount; }
    virtual unsigned MailboxRecentCount() { return m_recentCount; }
    virtual unsigned MailboxFirstUnseen() { return m_firstUnseen; }
    virtual std::string GetMailboxUserPath() const ;
    virtual MailStore::MAIL_STORE_RESULT MailboxFlushBuffers(NUMBER_LIST *nowGone);
    virtual MailStore::MAIL_STORE_RESULT MailboxUpdateStats(NUMBER_LIST *nowGone);
    virtual void BuildMailboxList(const char *ref, const char *pattern, MAILBOX_LIST *result, bool listAll);
    virtual ~MailStoreMbox();
    // This deletes a message in a mail box
    virtual MailStore::MAIL_STORE_RESULT DeleteMessage(const std::string &MailboxName, size_t uid);

private:
    const char *m_homeDirectory;
    const char *m_inboxPath;
    unsigned m_mailboxMessageCount;
    unsigned m_recentCount;
    unsigned m_firstUnseen;
    unsigned m_uidValidity;
    unsigned m_uidNext;
    bool m_isOpen;
    bool m_isDirty;
    std::ofstream *m_outFile;
    void ListAll(const char *pattern, MAILBOX_LIST *result);
    void ListSubscribed(const char *pattern, MAILBOX_LIST *result);
    bool isMailboxInteresting(const std::string path);
    bool ListAllHelper(const regex_t *compiled_regex, const char *home_directory, const char *working_dir, MAILBOX_LIST *result, int maxdepth);
    bool ParseMessage(std::ifstream &inFile, bool firstMessage, unsigned &messageCount, unsigned &recentCount, unsigned &uidNext,
		      unsigned &firstUnseen, unsigned &uidValidity);
};

#endif // _MAILSTOREMBOX_HPP_INCLUDED_
