#include <clotho/internetserver.hpp>

#include "mailstorelocktest.hpp"
#include "imapsession.hpp"

LockState g_lockState;

bool LockState::mailboxAlreadyLocked(void) {
  bool result = (0 < m_retryCount);
  --m_retryCount;
  return result;
}

MailStoreLockTest::MailStoreLockTest(ImapSession *session) : MailStore(session) {
  m_isLocked = false;
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::internalLockLogic(void) {
  MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
  if (!m_isLocked && g_lockState.mailboxAlreadyLocked()) {
    result = MailStore::CANNOT_COMPLETE_ACTION;
  }
  return result;
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::createMailbox(const std::string &FullName) {
  return internalLockLogic();
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::deleteMailbox(const std::string &FullName) {
  return internalLockLogic();
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::renameMailbox(const std::string &SourceName, const std::string &DestinationName) {
  return internalLockLogic();
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::mailboxClose() {
  return internalLockLogic();
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::addMessageToMailbox(const std::string &FullName, uint8_t *data, size_t length,
								    DateTime &createTime, uint32_t messageFlags, size_t *newUid) {
  return internalLockLogic();
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::appendDataToMessage(const std::string &MailboxName, size_t uid, uint8_t *data, size_t length) {
  return internalLockLogic();
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::doneAppendingDataToMessage(const std::string &MailboxName, size_t uid) {
  return internalLockLogic();
}

unsigned MailStoreLockTest::serialNumber()
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
MailStore::MAIL_STORE_RESULT MailStoreLockTest::mailboxOpen(const std::string &FullName, bool readWrite) {
  return internalLockLogic();
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::listDeletedMessages(NUMBER_SET *uidsToBeExpunged) {
  return internalLockLogic();
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::expungeThisUid(unsigned long uid) {
  return internalLockLogic();
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::getMailboxCounts(const std::string &FullName, uint32_t which, unsigned &messageCount,
								 unsigned &recentCount, unsigned &uidLast, unsigned &uidValidity,
								 unsigned &firstUnseen) {
  return internalLockLogic();
}


const DateTime &MailStoreLockTest::messageInternalDate(const unsigned long uid) {
  static DateTime now;
  return now;
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::messageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags) {
  return internalLockLogic();
}


std::string MailStoreLockTest::mailboxUserPath() const {
  // SYZYGY -- what is this for?
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::mailboxFlushBuffers(void) {
  return internalLockLogic();
}

MailStore::MAIL_STORE_RESULT MailStoreLockTest::mailboxUpdateStats(NUMBER_SET *nowGone) {
  return mailboxFlushBuffers();
}


void MailStoreLockTest::mailboxList(const std::string &pattern, MAILBOX_LIST *result, bool listAll) {
}



MailStore::MAIL_STORE_RESULT MailStoreLockTest::subscribeMailbox(const std::string &MailboxName, bool isSubscribe) {
  return internalLockLogic();
}


MailStoreLockTest::~MailStoreLockTest() {
  if (m_isLocked) {
    // SYZYGY throw an exception
  }
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::deleteMessage(const std::string &MailboxName, unsigned long uid) {
  return internalLockLogic();
}

MailMessage::MAIL_MESSAGE_RESULT MailStoreLockTest::messageData(MailMessage **message, unsigned long uid) {
  MailMessage::MAIL_MESSAGE_RESULT result = MailMessage::SUCCESS;
  return result;
}

size_t MailStoreLockTest::bufferLength(unsigned long uid) {
  size_t result = 0;

  // I'm probably going to have only a single fixed message that violates most of the IMAP
  // semantics because it's not intended to do anything but test the locking logic in the
  // ImapSession class

  return result;
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::openMessageFile(unsigned long uid) {
  return internalLockLogic();
}

// This function must replace LF's with CRLF's and it must ignore the headers that don't belong,
// it must return false at EOF or when it read's a "From" line, and it must un-quote quoted "From" lines
// up to length characters are read into the buffer pointed to by buff starting at an offset of 'offset'
// relative to the start of the message as it was originally received
// Oh, and it also must not return the last linefeed.
size_t MailStoreLockTest::readMessage(char *buff, size_t offset, size_t length) {
  size_t destPtr = 0;
  return destPtr;
}

void MailStoreLockTest::closeMessageFile(void) {
}


const SEARCH_RESULT *MailStoreLockTest::searchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate) {
  SEARCH_RESULT *result = new SEARCH_RESULT;
  return result;
}


const std::string MailStoreLockTest::generateUrl(const std::string MailboxName) const {
  std::string result = "locktest:///";
  return result;
}


MailStoreLockTest *MailStoreLockTest::clone(void) {
  return new MailStoreLockTest(m_session);
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::lock(void) {
  MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;

  if (!m_isLocked && g_lockState.mailboxAlreadyLocked()) {
    result = MailStore::CANNOT_COMPLETE_ACTION;
  }
  else {
    m_isLocked = true;
  }
  return result;
}


MailStore::MAIL_STORE_RESULT MailStoreLockTest::unlock(void) {
  MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;

  m_isLocked = false;
  return MailStore::SUCCESS;
}
