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

#if !defined(_MAILSTOREINVALID_HPP_INCLUDED_)
#define _MAILSTOREINVALID_HPP_INCLUDED_

#include "mailstore.hpp"

class MailStoreInvalid : public MailStore {
public:
  typedef enum {
    PERSONAL,
    OTHERS,
    SHARED
  } NAMESPACE_TYPES;
	
  MailStoreInvalid(ImapSession *session);
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
  virtual unsigned uidValidityNumber();
  virtual MailStore::MAIL_STORE_RESULT mailboxOpen(const std::string &MailboxName, bool readWrite = true);
  virtual MailStore::MAIL_STORE_RESULT listDeletedMessages(NUMBER_SET *uidsToBeExpunged);
  virtual MailStore::MAIL_STORE_RESULT expungeThisUid(unsigned long uid);

  virtual MailStore::MAIL_STORE_RESULT getMailboxCounts(const std::string &MailboxName, uint32_t which, unsigned &messageCount,
							unsigned &recentCount, unsigned &uidNext, unsigned &uidValidity,
							unsigned &firstUnseen);

  virtual unsigned mailboxMessageCount();
  virtual unsigned mailboxRecentCount();
  virtual unsigned mailboxFirstUnseen();

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
  virtual ~MailStoreInvalid();
  // This deletes a message in a mail box
  virtual MailStore::MAIL_STORE_RESULT deleteMessage(const std::string &MailboxName, unsigned long uid);
  virtual MailMessage::MAIL_MESSAGE_RESULT messageData(MailMessage **message, unsigned long uid);
  virtual size_t bufferLength(unsigned long uid);
  virtual MAIL_STORE_RESULT openMessageFile(unsigned long uid);
  virtual size_t readMessage(char *buff, size_t offset, size_t length);
  virtual void closeMessageFile(void);
  virtual const SEARCH_RESULT *searchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate);
  virtual const std::string generateUrl(const std::string MailboxName) const;
  virtual MailStoreInvalid *clone(void);
  virtual MailStore::MAIL_STORE_RESULT lock(void);
  virtual MailStore::MAIL_STORE_RESULT unlock(void);

private:
};

#endif // _MAILSTOREINVALID_HPP_INCLUDED_
