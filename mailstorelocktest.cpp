
#include "mailstorelocktest.hpp"
#include "internetserver.hpp"
#include "imapsession.hpp"

MailStoreLockTest::MailStoreLockTest(ImapSession *session) : MailStore(session) {
    m_isLocked = false;
    m_lockCount = 0;
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::CreateMailbox(const std::string &FullName) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
    return result;
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::DeleteMailbox(const std::string &FullName) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
    return result;
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::RenameMailbox(const std::string &SourceName, const std::string &DestinationName) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
    return result;
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::MailboxClose() {
    return MailStore::SUCCESS;
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::AddMessageToMailbox(const std::string &FullName, uint8_t *data, size_t length,
						 DateTime &createTime, uint32_t messageFlags, size_t *newUid) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
    return result;
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::AppendDataToMessage(const std::string &MailboxName, size_t uid, uint8_t *data, size_t length) {
    return MailStore::SUCCESS; // SYZYGY 
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::DoneAppendingDataToMessage(const std::string &MailboxName, size_t uid) {
    return MailStore::SUCCESS; // SYZYGY
}

unsigned MailStoreLockTest::GetSerialNumber()
{
    return 1;
}


// Fundamentally, the proximate purpose behind MailboxOpen is to populate these values:
//    mailboxMessageCount
//    recentCount
//    firstUnseen
//    uidLast
//    uidValidity
// and to get a list of the flags and permanent flags
// to do that, I need to at least partly parse the entire file, I may want to parse and cache 
// the results of that parsing for things like message sequence numbers, UID's, and offsets in the
// file
MailStore::MAIL_STORE_RESULT MailStoreLockTest::MailboxOpen(const std::string &FullName, bool readWrite) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
    return result;
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::ListDeletedMessages(NUMBER_SET *uidsToBeExpunged) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
    return result;
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::ExpungeThisUid(unsigned long uid) {
    MailStore::MAIL_STORE_RESULT result = MailStore::MESSAGE_NOT_FOUND;
    return result;
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::GetMailboxCounts(const std::string &FullName, uint32_t which, unsigned &messageCount,
							     unsigned &recentCount, unsigned &uidLast, unsigned &uidValidity,
							     unsigned &firstUnseen) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
    return result;
}


const DateTime &MailStoreLockTest::MessageInternalDate(const unsigned long uid) {
    static DateTime now;
    return now;
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::MessageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
    return result;
}


std::string MailStoreLockTest::GetMailboxUserPath() const {
    // SYZYGY -- what is this for?
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::MailboxFlushBuffers(void) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
    return result;
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::MailboxUpdateStats(NUMBER_SET *nowGone) {
    return MailboxFlushBuffers();
}


void MailStoreLockTest::BuildMailboxList(const std::string &pattern, MAILBOX_LIST *result, bool listAll) {
}



MailStore::MAIL_STORE_RESULT MailStoreLockTest::SubscribeMailbox(const std::string &MailboxName, bool isSubscribe) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
    return result;
}


MailStoreLockTest::~MailStoreLockTest() {
    if (m_isLocked) {
	// SYZYGY throw an exception
    }
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::DeleteMessage(const std::string &MailboxName, unsigned long uid) {
    return MailStore::SUCCESS;
}

MailMessage::MAIL_MESSAGE_RESULT MailStoreLockTest::GetMessageData(MailMessage **message, unsigned long uid) {
    MailMessage::MAIL_MESSAGE_RESULT result = MailMessage::SUCCESS;
    return result;
}

size_t MailStoreLockTest::GetBufferLength(unsigned long uid) {
    size_t result = 0;

    // I'm probably going to have only a single fixed message that violates most of the IMAP
    // semantics because it's not intended to do anything but test the locking logic in the
    // ImapSession class

    return result;
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::OpenMessageFile(unsigned long uid) {
    return MailStore::SUCCESS;
}

// This function must replace LF's with CRLF's and it must ignore the headers that don't belong,
// it must return false at EOF or when it read's a "From" line, and it must un-quote quoted "From" lines
// up to length characters are read into the buffer pointed to by buff starting at an offset of 'offset'
// relative to the start of the message as it was originally received
// Oh, and it also must not return the last linefeed.
size_t MailStoreLockTest::ReadMessage(char *buff, size_t offset, size_t length) {
    size_t destPtr = 0;
    return destPtr;
}

void MailStoreLockTest::CloseMessageFile(void) {
}


const SEARCH_RESULT *MailStoreLockTest::SearchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate) {
    SEARCH_RESULT *result = new SEARCH_RESULT;
    return result;
}


const std::string MailStoreLockTest::GenerateUrl(const std::string MailboxName) const {
    std::string result = "locktest:///";
    return result;
}


MailStoreLockTest *MailStoreLockTest::clone(void) {
    return new MailStoreLockTest(m_session);
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::MailboxLock(void) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;

    if (m_isLocked) {
	result = MailStore::CANNOT_COMPLETE_ACTION;
    }
    else {
	m_isLocked = true;
    }
    return result;
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::MailboxUnlock(void) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;

    if (m_isLocked) {
	m_isLocked = true;
    }
    else {
	result = MailStore::CANNOT_COMPLETE_ACTION;
    }
    return result;
}
