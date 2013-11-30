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

#if !defined(_MAILSTOREMBOX_HPP_INCLUDED_)
#define _MAILSTOREMBOX_HPP_INCLUDED_

#include <fstream>

#include <regex.h>

#include "mailstore.hpp"

class MailStoreMbox : public MailStore {
public:
  MailStoreMbox(ImapSession *session);
  virtual MailStore::MAIL_STORE_RESULT createMailbox(const std::string &MailboxName);
  virtual MailStore::MAIL_STORE_RESULT deleteMailbox(const std::string &MailboxName);
  virtual MailStore::MAIL_STORE_RESULT renameMailbox(const std::string &SourceName, const std::string &DestinationName);
  virtual MailStore::MAIL_STORE_RESULT mailboxClose();
  virtual MailStore::MAIL_STORE_RESULT subscribeMailbox(const std::string &MailboxName, bool isSubscribe);
  virtual MailStore::MAIL_STORE_RESULT addMessageToMailbox(const std::string &MailboxName, const uint8_t *data, size_t length,
							   DateTime &createTime, uint32_t messageFlags, size_t *newUid = NULL);
  virtual MailStore::MAIL_STORE_RESULT appendDataToMessage(const std::string &MailboxName, size_t uid, const uint8_t *data, size_t length);
  virtual MailStore::MAIL_STORE_RESULT doneAppendingDataToMessage(const std::string &MailboxName, size_t uid);
  virtual unsigned serialNumber();
  virtual unsigned uidValidityNumber() { return m_uidValidity; }
  virtual MailStore::MAIL_STORE_RESULT mailboxOpen(const std::string &MailboxName, bool readWrite = true);
  virtual MailStore::MAIL_STORE_RESULT listDeletedMessages(NUMBER_SET *uidsToBeExpunged);
  virtual MailStore::MAIL_STORE_RESULT expungeThisUid(unsigned long uid);

  virtual MAIL_STORE_RESULT getMailboxCounts(const std::string &MailboxName, uint32_t which, unsigned &messageCount,
					     unsigned &recentCount, unsigned &uidNext, unsigned &uidValidity, unsigned &firstUnseen);

  virtual unsigned mailboxMessageCount() { return m_mailboxMessageCount; }
  virtual unsigned mailboxRecentCount() { return m_recentCount; }
  virtual unsigned mailboxFirstUnseen() { return m_firstUnseen; }

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
  virtual ~MailStoreMbox();
  // This deletes a message in a mail box
  virtual MailStore::MAIL_STORE_RESULT deleteMessage(const std::string &MailboxName, unsigned long uid);
  virtual MailMessage::MAIL_MESSAGE_RESULT messageData(MailMessage **message, unsigned long uid);
  virtual size_t bufferLength(unsigned long uid);
  virtual MAIL_STORE_RESULT openMessageFile(unsigned long uid);
  virtual size_t readMessage(char *buff, size_t offset, size_t length);
  virtual void closeMessageFile(void);
  virtual const SEARCH_RESULT *searchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate);
  virtual const std::string generateUrl(const std::string MailboxName) const;
  virtual MailStoreMbox *clone(void);
  virtual MailStore::MAIL_STORE_RESULT lock(void);
  virtual MailStore::MAIL_STORE_RESULT unlock(void);

private:
  typedef struct {
    unsigned uid;
    uint32_t flags;
    std::fstream::pos_type start;
    MailMessage *messageData;
    size_t rfc822MessageSize;
    bool isNotified;
    bool isExpunged;
    bool isDirty;
    DateTime internalDate;
  } MessageIndex_t;

  typedef std::vector<MessageIndex_t> MESSAGE_INDEX;
  MESSAGE_INDEX m_messageIndex;
  const std::string *m_homeDirectory;
  const std::string *m_inboxPath;
  unsigned m_mailboxMessageCount;
  unsigned m_recentCount;
  unsigned m_firstUnseen;
  unsigned m_uidValidity;
  unsigned m_uidLast;
  bool m_hasHiddenMessage;
  bool m_hasExpungedMessage;
  bool m_isDirty;
  std::ofstream *m_outFile;
  // Appendstate is used as part of the append process.  It's used to detect any "\n>*From " strings in messages so that I can 
  // properly quote them as the message is imported.  I need to create a state transition diagram, somewhere.
  int m_appendState;
  time_t m_lastMtime; // The mtime of the file the last time it was at a known good state.
  void listAll(const std::string &pattern, MAILBOX_LIST *result);
  void listSubscribed(const std::string &pattern, MAILBOX_LIST *result);
  bool isMailboxInteresting(const std::string path);
  bool listAllHelper(const regex_t *compiled_regex, const char *home_directory, const char *working_dir, MAILBOX_LIST *result, int maxdepth);
  bool parseMessage(std::ifstream &inFile, bool firstMessage, bool &countMessage, unsigned &uidValidity, unsigned &uidNext, MessageIndex_t &messageMetaData);
  void addDataToMessageFile(const uint8_t *data, size_t length);
  MailStore::MAIL_STORE_RESULT flush(void);
  std::ifstream m_inFile;
  unsigned long m_readingMsn;
};

#endif // _MAILSTOREMBOX_HPP_INCLUDED_
