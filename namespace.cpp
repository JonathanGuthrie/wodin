#include "namespace.hpp"

Namespace::Namespace(ImapSession *session) : MailStore(session) {
    inboxPath = strdup(usersInboxPath);
    homeDirectory = strdup(usersHomeDirectory);
}

MailStore::MAIL_STORE_RESULT Namespace::CreateMailbox(const std::string &FullName) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT Namespace::DeleteMailbox(const std::string &FullName) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT Namespace::RenameMailbox(const std::string &SourceName, const std::string &DestinationName) {
    return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT Namespace::MailboxClose() {
    return GENERAL_FAILURE;
}

void Namespace::BuildMailboxList(const char *ref, const char *pattern, MAILBOX_LIST *result, bool listAll) {
}

MailStore::MAIL_STORE_RESULT Namespace::SubscribeMailbox(const std::string &MailboxName, bool isSubscribe) {
}

// The next seven methods only have meaning if a mailbox has been opened, something I expect to be 
// enforced by the IMAP server logic because they're only meaningful in the selected state
unsigned Namespace::GetSerialNumber() {
}

unsigned Namespace::GetNextSerialNumber() {
}

unsigned Namespace::MailboxMessageCount(const std::string &MailboxName) {
}

unsigned Namespace::MailboxRecentCount(const std::string &MailboxName) {
}

unsigned Namespace::MailboxFirstUnseen(const std::string &MailboxName) {
}

std::string Namespace::GetMailboxUserPath() const {
}

MailStore::MAIL_STORE_RESULT Namespace::MailboxUpdateStats(NUMBER_LIST *nowGone) {
    return GENERAL_FAILURE;
}

Namespace::~MailStoreMbox() {
}

