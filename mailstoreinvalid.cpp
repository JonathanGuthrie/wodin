#include "mailstoreinvalid.hpp"

MailStoreInvalid::MailStoreInvalid(ImapSession *session) : MailStore(session) {
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::createMailbox(const std::string &FullName) {
  return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::deleteMailbox(const std::string &FullName) {
  return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::renameMailbox(const std::string &SourceName, const std::string &DestinationName) {
  return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::mailboxClose() {
  return GENERAL_FAILURE;
}

void MailStoreInvalid::mailboxList(const std::string &pattern, MAILBOX_LIST *result, bool listAll) {
  // The nothing that MAILBOX_LIST starts as is is appropriate here.
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::subscribeMailbox(const std::string &MailboxName, bool isSubscribe) {
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::addMessageToMailbox(const std::string &MailboxName, uint8_t *data, size_t length,
								   DateTime &createTime, uint32_t messageFlags, size_t *newUid ) {
  return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::appendDataToMessage(const std::string &MailboxName, size_t uid, uint8_t *data, size_t length) {
  return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::doneAppendingDataToMessage(const std::string &MailboxName, size_t uid) {
  return GENERAL_FAILURE;
}

// The next seven methods only have meaning if a mailbox has been opened, something I expect to be 
// enforced by the IMAP server logic because they're only meaningful in the selected state
unsigned MailStoreInvalid::serialNumber() {
  return 0;
}

unsigned MailStoreInvalid::uidValidityNumber() {
  return 0;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::mailboxOpen(const std::string &MailboxName, bool readWrite) {
  return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::listDeletedMessages(NUMBER_SET *nowGone) {
  return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::expungeThisUid(unsigned long uid) {
  return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::getMailboxCounts(const std::string &MailboxName, uint32_t which, unsigned &messageCount,
								unsigned &recentCount, unsigned &uidNext, unsigned &uidValidity,
								unsigned &firstUnseen) {
  return GENERAL_FAILURE;
}

unsigned MailStoreInvalid::mailboxMessageCount() {
  return 0;
}

unsigned MailStoreInvalid::mailboxRecentCount() {
  return 0;
}

unsigned MailStoreInvalid::mailboxFirstUnseen() {
  return 0;
}

const DateTime &MailStoreInvalid::messageInternalDate(const unsigned long uid) {
  // SYZYGY -- I need something like a void cast to DateTime that returns an error
  static DateTime now;
  return now;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::messageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags) {
  return GENERAL_FAILURE;
}

std::string MailStoreInvalid::mailboxUserPath() const {
  return "";
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::mailboxFlushBuffers(void) {
  return GENERAL_FAILURE;
}

MailStore::MAIL_STORE_RESULT MailStoreInvalid::mailboxUpdateStats(NUMBER_SET *nowGone) {
  return GENERAL_FAILURE;
}

MailStoreInvalid::~MailStoreInvalid() {
}


MailStore::MAIL_STORE_RESULT MailStoreInvalid::deleteMessage(const std::string &MailboxName, unsigned long uid) {
  return GENERAL_FAILURE;
}

MailMessage::MAIL_MESSAGE_RESULT MailStoreInvalid::messageData(MailMessage **message, unsigned long uid) {
  *message = NULL;
  return MailMessage::MESSAGE_DOESNT_EXIST;
}


MailStore::MAIL_STORE_RESULT MailStoreInvalid::openMessageFile(unsigned long uid) {
  return MailStore::GENERAL_FAILURE;
}

size_t MailStoreInvalid::bufferLength(unsigned long uid) {
  return 0;
}

size_t MailStoreInvalid::readMessage(char *buff, size_t offset, size_t length) {
  return 0;
}

void MailStoreInvalid::closeMessageFile(void) {
}

const SEARCH_RESULT *MailStoreInvalid::searchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate) {
  return NULL;
}


const std::string MailStoreInvalid::generateUrl(const std::string MailboxName) const {
  std::string result;
  result = "invalid:///" + MailboxName;
  return result;
}


MailStoreInvalid *MailStoreInvalid::clone(void) {
  return new MailStoreInvalid(m_session);
}


MailStore::MAIL_STORE_RESULT MailStoreInvalid::lock(void) {
  return GENERAL_FAILURE;
}


MailStore::MAIL_STORE_RESULT MailStoreInvalid::unlock(void) {
  return GENERAL_FAILURE;
}
