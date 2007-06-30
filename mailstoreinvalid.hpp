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

    // This updates the flags associated with the email message
    // of 'orig' is the original flag set, then the final flag set is 
    // orMask | (andMask & orig)
    // The final flag set is returned in flags
    virtual MailStore::MAIL_STORE_RESULT MessageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags);

    virtual std::string GetMailboxUserPath() const ;
    virtual MailStore::MAIL_STORE_RESULT MailboxFlushBuffers(NUMBER_LIST *nowGone);
    virtual MailStore::MAIL_STORE_RESULT MailboxUpdateStats(NUMBER_LIST *nowGone);
    virtual void BuildMailboxList(const std::string &pattern, MAILBOX_LIST *result, bool listAll);
    virtual ~MailStoreInvalid();
    // This deletes a message in a mail box
    virtual MailStore::MAIL_STORE_RESULT DeleteMessage(const std::string &MailboxName, unsigned long uid);
    virtual MailMessage::MAIL_MESSAGE_RESULT GetMessageData(MailMessage **message, unsigned long uid);
    virtual size_t GetBufferLength(unsigned long uid);
    virtual MAIL_STORE_RESULT OpenMessageFile(unsigned long uid);
    virtual size_t ReadMessage(char *buff, size_t offset, size_t length);
    virtual void CloseMessageFile(void);
    virtual const SEARCH_RESULT *SearchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate);

private:
};

#endif // _MAILSTOREINVALID_HPP_INCLUDED_
