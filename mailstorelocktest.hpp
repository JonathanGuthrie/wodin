/*
 * Copyright 2013 Jonathan R. Guthrie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !defined(_MAILSTORELOCKTEST_HPP_INCLUDED_)
#define _MAILSTORELOCKTEST_HPP_INCLUDED_

#include <fstream>

#include <regex.h>

#include "mailstore.hpp"

/*
 * when mailboxAlreadyLocked is called, it will return true if the count is 
 * greater than 0 and subtract one from m_retryCount for next time
 */
class LockState {
public:
  enum Bits {
    TestOpen        = 0x00000001,
    TestClose       = 0x00000002,
    Fail            = 0x00000004,
    TestUpdateStats = 0x00000008,
  };
  bool mailboxAlreadyLocked(void);
  int retryCount(void) { return m_retryCount; }
  void retryCount(int count) { m_retryCount = count; }
  void testControl(uint32_t bitfield) { m_testControl = bitfield; }
  uint32_t testControl(void) { return m_testControl; }

private:
  int m_retryCount;
  uint32_t m_testControl;
};

extern LockState g_lockState;

/*
 * The MailStoreLockTest class implements a mail store that is only useful to test the lock handling of the
 * IMAP session handler
 */
class MailStoreLockTest : public MailStore {
public:
  MailStoreLockTest(ImapSession *session);
  virtual MailStore::MAIL_STORE_RESULT createMailbox(const std::string &MailboxName);
  virtual MailStore::MAIL_STORE_RESULT deleteMailbox(const std::string &MailboxName);
  virtual MailStore::MAIL_STORE_RESULT renameMailbox(const std::string &SourceName, const std::string &DestinationName);
  virtual MailStore::MAIL_STORE_RESULT mailboxClose();
  virtual MailStore::MAIL_STORE_RESULT subscribeMailbox(const std::string &MailboxName, bool isSubscribe);
  virtual MailStore::MAIL_STORE_RESULT addMessageToMailbox(const std::string &MailboxName, uint8_t *data, size_t length,
							   DateTime &createTime, uint32_t messageFlags, size_t *newUid = NULL);
  virtual MailStore::MAIL_STORE_RESULT appendDataToMessage(const std::string &MailboxName, size_t uid, uint8_t *data, size_t length);
  virtual MailStore::MAIL_STORE_RESULT doneAppendingDataToMessage(const std::string &MailboxName, size_t uid);
  virtual unsigned serialNumber();
  virtual unsigned uidValidityNumber() { return 0; }
  virtual MailStore::MAIL_STORE_RESULT mailboxOpen(const std::string &MailboxName, bool readWrite = true);
  virtual MailStore::MAIL_STORE_RESULT listDeletedMessages(NUMBER_SET *uidsToBeExpunged);
  virtual MailStore::MAIL_STORE_RESULT expungeThisUid(unsigned long uid);

  virtual MAIL_STORE_RESULT getMailboxCounts(const std::string &MailboxName, uint32_t which, unsigned &messageCount,
					     unsigned &recentCount, unsigned &uidNext, unsigned &uidValidity, unsigned &firstUnseen);

  virtual unsigned mailboxMessageCount() { return 1; }
  virtual unsigned mailboxRecentCount() { return 0; }
  virtual unsigned mailboxFirstUnseen() { return 0; }

  virtual const DateTime &messageInternalDate(const unsigned long uid);

  // This updates the flags associated with the email message
  // of 'orig' is the original flag set, then the final flag set is 
  // orMask | (andMask & orig)
  // The final flag set is returned in flags
  virtual MailStore::MAIL_STORE_RESULT messageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags);

  virtual std::string mailboxUserPath() const ;
  virtual MailStore::MAIL_STORE_RESULT mailboxFlushBuffers(void);
  virtual MailStore::MAIL_STORE_RESULT mailboxUpdateStats(NUMBER_SET *nowGone);
  virtual void mailboxList(const std::string &pattern, MAILBOX_LIST *result, bool listAll);
  virtual ~MailStoreLockTest();
  // This deletes a message in a mail box
  virtual MailStore::MAIL_STORE_RESULT deleteMessage(const std::string &MailboxName, unsigned long uid);
  virtual MailMessage::MAIL_MESSAGE_RESULT messageData(MailMessage **message, unsigned long uid);
  virtual size_t bufferLength(unsigned long uid);
  virtual MAIL_STORE_RESULT openMessageFile(unsigned long uid);
  virtual size_t readMessage(char *buff, size_t offset, size_t length);
  virtual void closeMessageFile(void);
  virtual const SEARCH_RESULT *searchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate);
  virtual const std::string generateUrl(const std::string MailboxName) const;
  virtual MailStoreLockTest *clone(void);
  virtual MailStore::MAIL_STORE_RESULT lock(void);
  virtual MailStore::MAIL_STORE_RESULT unlock(void);

private:
  bool m_isLocked;
  MailStore::MAIL_STORE_RESULT internalLockLogic(void);
};

#endif // _MAILSTORELOCKTEST_HPP_INCLUDED_
