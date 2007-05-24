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

// The next seven methods only have meaning if a mailbox has been opened, something I expect to be 
// enforced by the IMAP server logic because they're only meaningful in the selected state
unsigned MailStoreInvalid::GetSerialNumber() {
    return 0;
}

unsigned MailStoreInvalid::GetNextSerialNumber() {
    return 0;
}

unsigned MailStoreInvalid::GetUidValidityNumber() {
    return 0;
}

unsigned MailStoreInvalid::MailboxMessageCount(const std::string &MailboxName) {
    return 0;
}

unsigned MailStoreInvalid::MailboxRecentCount(const std::string &MailboxName) {
    return 0;
}

unsigned MailStoreInvalid::MailboxFirstUnseen(const std::string &MailboxName) {
    return 0;
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

MailStore::MAIL_STORE_RESULT MailStoreInvalid::MailboxUpdateStats(NUMBER_LIST *nowGone) {
    return GENERAL_FAILURE;
}

MailStoreInvalid::~MailStoreInvalid() {
}
