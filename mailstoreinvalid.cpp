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

void MailStoreInvalid::BuildMailboxList(const char *ref, const char *pattern, MAILBOX_LIST *result, bool listAll) {
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

std::string MailStoreInvalid::GetMailboxUserPath() const {
    return "";
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::MailboxFlushBuffers(NUMBER_LIST *nowGone) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::MailboxUpdateStats(NUMBER_LIST *nowGone) {
    return GENERAL_FAILURE;
}

MailStoreInvalid::~MailStoreInvalid() {
}


MailStore::MAIL_STORE_RESULT MailStoreInvalid::DeleteMessage(const std::string &MailboxName, size_t uid) {
    return GENERAL_FAILURE;
}
