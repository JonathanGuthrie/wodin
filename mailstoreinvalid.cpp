#include "mailstoreinvalid.hpp"

MailStoreInvalid::MailStoreInvalid(ImapSession *session) : MailStore(session) {
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::CreateMailbox(const std::string &FullName) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::DeleteMailbox(const std::string &FullName) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::RenameMailbox(const std::string &SourceName, const std::string &DestinationName) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::MailboxClose() {
    return GENERAL_FAILURE;
}

void MailStoreInvalid::BuildMailboxList(const std::string &pattern, MAILBOX_LIST *result, bool listAll) {
    // The nothing that MAILBOX_LIST starts as is is appropriate here.
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::SubscribeMailbox(const std::string &MailboxName, bool isSubscribe) {
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::AddMessageToMailbox(const std::string &MailboxName, uint8_t *data, size_t length,
								   DateTime &createTime, uint32_t messageFlags, size_t *newUid ) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::AppendDataToMessage(const std::string &MailboxName, size_t uid, uint8_t *data, size_t length) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::DoneAppendingDataToMessage(const std::string &MailboxName, size_t uid) {
    return GENERAL_FAILURE;
}

// The next seven methods only have meaning if a mailbox has been opened, something I expect to be 
// enforced by the IMAP server logic because they're only meaningful in the selected state
unsigned MailStoreInvalid::GetSerialNumber() {
    return 0;
}

unsigned MailStoreInvalid::GetUidValidityNumber() {
    return 0;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::MailboxOpen(const std::string &MailboxName, bool readWrite) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::ListDeletedMessages(NUMBER_SET *nowGone) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::ExpungeThisUid(unsigned long uid) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::GetMailboxCounts(const std::string &MailboxName, uint32_t which, unsigned &messageCount,
								unsigned &recentCount, unsigned &uidNext, unsigned &uidValidity,
								unsigned &firstUnseen) {
    return GENERAL_FAILURE;
}

unsigned MailStoreInvalid::MailboxMessageCount() {
    return 0;
}

unsigned MailStoreInvalid::MailboxRecentCount() {
    return 0;
}

unsigned MailStoreInvalid::MailboxFirstUnseen() {
    return 0;
}

const DateTime &MailStoreInvalid::MessageInternalDate(const unsigned long uid) {
    // SYZYGY -- I need something like a void cast to DateTime that returns an error
    static DateTime now;
    return now;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::MessageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags) {
    return GENERAL_FAILURE;
}

std::string MailStoreInvalid::GetMailboxUserPath() const {
    return "";
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::MailboxFlushBuffers(void) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::MailboxUpdateStats(NUMBER_SET *nowGone) {
    return GENERAL_FAILURE;
}

MailStoreInvalid::~MailStoreInvalid() {
}


MailStore::MAIL_STORE_RESULT MailStoreInvalid::DeleteMessage(const std::string &MailboxName, unsigned long uid) {
    return GENERAL_FAILURE;
}

MailMessage::MAIL_MESSAGE_RESULT MailStoreInvalid::GetMessageData(MailMessage **message, unsigned long uid) {
    *message = NULL;
    return MailMessage::MESSAGE_DOESNT_EXIST;
}


MailStore::MAIL_STORE_RESULT MailStoreInvalid::OpenMessageFile(unsigned long uid) {
    return MailStore::GENERAL_FAILURE;
}

size_t MailStoreInvalid::GetBufferLength(unsigned long uid) {
    return 0;
}

size_t MailStoreInvalid::ReadMessage(char *buff, size_t offset, size_t length) {
    return 0;
}

void MailStoreInvalid::CloseMessageFile(void) {
}

const SEARCH_RESULT *MailStoreInvalid::SearchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate) {
    return NULL;
}


const std::string MailStoreInvalid::GenerateUrl(const std::string MailboxName) const {
    std::string result;
    result = "invalid:///" + MailboxName;
    return result;
}


MailStoreInvalid *MailStoreInvalid::clone(void) {
    return new MailStoreInvalid(m_session);
}
