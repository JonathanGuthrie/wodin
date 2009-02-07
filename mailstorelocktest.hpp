#if !defined(_MAILSTORELOCKTEST_HPP_INCLUDED_)
#define _MAILSTORELOCKTEST_HPP_INCLUDED_

#include <fstream>

#include <regex.h>

#include "mailstore.hpp"

/*
 * The MailStoreLockTest class implements a mail store that is only useful to test the lock handling of the
 * IMAP session handler
 */
class MailStoreLockTest : public MailStore
{
public:
    MailStoreLockTest(ImapSession *session);
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
    virtual unsigned GetUidValidityNumber() { return 0; }
    virtual MailStore::MAIL_STORE_RESULT MailboxOpen(const std::string &MailboxName, bool readWrite = true);
    virtual MailStore::MAIL_STORE_RESULT ListDeletedMessages(NUMBER_SET *uidsToBeExpunged);
    virtual MailStore::MAIL_STORE_RESULT ExpungeThisUid(unsigned long uid);

    virtual MAIL_STORE_RESULT GetMailboxCounts(const std::string &MailboxName, uint32_t which, unsigned &messageCount,
					       unsigned &recentCount, unsigned &uidNext, unsigned &uidValidity, unsigned &firstUnseen);

    virtual unsigned MailboxMessageCount() { return 0; }
    virtual unsigned MailboxRecentCount() { return 0; }
    virtual unsigned MailboxFirstUnseen() { return 0; }

    virtual const DateTime &MessageInternalDate(const unsigned long uid);

    // This updates the flags associated with the email message
    // of 'orig' is the original flag set, then the final flag set is 
    // orMask | (andMask & orig)
    // The final flag set is returned in flags
    virtual MailStore::MAIL_STORE_RESULT MessageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags);

    virtual std::string GetMailboxUserPath() const ;
    virtual MailStore::MAIL_STORE_RESULT MailboxFlushBuffers(void);
    virtual MailStore::MAIL_STORE_RESULT MailboxUpdateStats(NUMBER_SET *nowGone);
    virtual void BuildMailboxList(const std::string &pattern, MAILBOX_LIST *result, bool listAll);
    virtual ~MailStoreLockTest();
    // This deletes a message in a mail box
    virtual MailStore::MAIL_STORE_RESULT DeleteMessage(const std::string &MailboxName, unsigned long uid);
    virtual MailMessage::MAIL_MESSAGE_RESULT GetMessageData(MailMessage **message, unsigned long uid);
    virtual size_t GetBufferLength(unsigned long uid);
    virtual MAIL_STORE_RESULT OpenMessageFile(unsigned long uid);
    virtual size_t ReadMessage(char *buff, size_t offset, size_t length);
    virtual void CloseMessageFile(void);
    virtual const SEARCH_RESULT *SearchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate);
    virtual const std::string GenerateUrl(const std::string MailboxName) const;
    virtual MailStoreLockTest *clone(void);
    virtual MailStore::MAIL_STORE_RESULT MailboxLock(void);
    virtual MailStore::MAIL_STORE_RESULT MailboxUnlock(void);

private:
    bool m_isLocked;
    int m_lockCount;
};

#endif // _MAILSTORELOCKTEST_HPP_INCLUDED_
