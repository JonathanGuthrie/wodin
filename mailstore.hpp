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

#if !defined(_MAILSTORE_HPP_INCLUDED_)
#define _MAILSTORE_HPP_INCLUDED_

#include <set>
#include <list>
#include <vector>
#include <string>

#include "datetime.hpp"
#include "mailmessage.hpp"

typedef struct {
  std::string name;
  uint32_t	attributes;  
} MAILBOX_NAME; 

typedef std::list<MAILBOX_NAME> MAILBOX_LIST;
typedef std::list<unsigned long> NUMBER_LIST;
typedef std::set<unsigned long> NUMBER_SET;
typedef std::vector<unsigned long> MSN_TO_UID;
typedef std::vector<unsigned long> SEARCH_RESULT;

class ImapSession;

class MailStore {
public:
  typedef enum {
    SUCCESS = 0,
    GENERAL_FAILURE,
    CANNOT_COMPLETE_ACTION,
    CANNOT_CREATE_INBOX,
    CANNOT_DELETE_INBOX,
    MAILBOX_ALREADY_EXISTS,
    MAILBOX_DOES_NOT_EXIST,
    MAILBOX_PATH_BAD,
    MAILBOX_ALREADY_SUBSCRIBED,
    MAILBOX_NOT_SUBSCRIBED,
    MAILBOX_IS_ACTIVE,
    MAILBOX_IS_NOT_LEAF,
    MAILBOX_NOT_SELECTABLE,
    MAILBOX_READ_ONLY,
    MAILBOX_NOT_OPEN,
    MESSAGE_FILE_OPEN_FAILED,
    MESSAGE_FILE_WRITE_FAILED,
    MESSAGE_NOT_FOUND,
    NAMESPACES_DONT_MATCH
  } MAIL_STORE_RESULT;

  typedef enum {
    IMAP_MESSAGE_ANSWERED = 0x01,
    IMAP_MESSAGE_FLAGGED = 0x02,
    IMAP_MESSAGE_DELETED = 0x04,
    IMAP_MESSAGE_SEEN = 0x08,
    IMAP_MESSAGE_DRAFT = 0x10,
    IMAP_MESSAGE_RECENT = 0x20
  } MAIL_STORE_FLAGS;

  typedef enum {
    IMAP_MBOX_NOSELECT = 0x01,
    // This might look a little strange, but there are three possible
    // states:  The server knows the mailbox is "interesting", the
    // server knows the mailbox is "uninteresting" and the server
    // can't tell.  Note that if the IMAP_MBOX_MARKED flag is set,
    // the state of IMAP_MBOX_UNMARKED will be ignored
    IMAP_MBOX_MARKED = 0x02,
    IMAP_MBOX_UNMARKED = 0x04,
    IMAP_MBOX_HASCHILDREN = 0x08,
    IMAP_MBOX_NOINFERIORS = 0x10
  } MAIL_BOX_FLAGS;

  MailStore(ImapSession *session);
  virtual MAIL_STORE_RESULT createMailbox(const std::string &MailboxName) = 0;
  virtual MAIL_STORE_RESULT deleteMailbox(const std::string &MailboxName) = 0;
  virtual MAIL_STORE_RESULT renameMailbox(const std::string &SourceName, const std::string &DestinationName) = 0;
  virtual MAIL_STORE_RESULT mailboxClose() = 0;
  // This function subscribes to the specified mailbox if isSubscribe is true, otherwise it 
  // unsubscribes from the mailbox
  virtual MAIL_STORE_RESULT subscribeMailbox(const std::string &MailboxName, bool isSubscribe) = 0;
  virtual MAIL_STORE_RESULT addMessageToMailbox(const std::string &MailboxName, const uint8_t *data, size_t length,
						DateTime &createTime, uint32_t messageFlags, size_t *newUid = NULL) = 0;
  virtual MAIL_STORE_RESULT appendDataToMessage(const std::string &MailboxName, size_t uid, const uint8_t *data, size_t length) = 0;
  virtual MAIL_STORE_RESULT doneAppendingDataToMessage(const std::string &MailboxName, size_t uid) = 0;
  virtual unsigned serialNumber() = 0;
  virtual unsigned uidValidityNumber() = 0;
  virtual MAIL_STORE_RESULT mailboxOpen(const std::string &MailboxName, bool readWrite = true) = 0;

  virtual MAIL_STORE_RESULT listDeletedMessages(NUMBER_SET *uidsToBeExpunged) = 0;
  virtual MAIL_STORE_RESULT expungeThisUid(unsigned long uid) = 0;
  virtual MAIL_STORE_RESULT getMailboxCounts(const std::string &MailboxName, uint32_t which, unsigned &messageCount,
					     unsigned &recentCount, unsigned &uidNext, unsigned &uidValidity, unsigned &firstUnseen) = 0;

  virtual unsigned mailboxMessageCount() = 0;
  virtual unsigned mailboxRecentCount() = 0;
  virtual unsigned mailboxFirstUnseen() = 0;

  virtual NUMBER_LIST mailboxMsnToUid(const NUMBER_LIST &msns);
  virtual unsigned long mailboxMsnToUid(const unsigned long msn);
  virtual NUMBER_LIST mailboxUidToMsn(const NUMBER_LIST &uids);
  virtual unsigned long mailboxUidToMsn(const unsigned long uid);

  virtual const DateTime &messageInternalDate(const unsigned long uid) = 0;

  // This stores a list of all UIDs in the system into msns
  virtual MAIL_STORE_RESULT messageList(SEARCH_RESULT &msns) const;

  // This updates the flags associated with the email message
  // of 'orig' is the original flag set, then the final flag set is 
  // orMask | (andMask & orig)
  // The final flag set is returned in flags
  virtual MAIL_STORE_RESULT messageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags) = 0;

  virtual std::string mailboxUserPath() const = 0;
  // MailboxFlushBuffers and MailboxUpdateStats updates statistics and also handles expunges on some
  // mail stores, which is why it has to return a list of expunged MSN's.  However, MailboxUpdateStats
  // doesn't do any expunges.  The difference is that MailboxFlushBuffers is called by the handler for
  // CHECK and MailboxUpdateStats is called by the handler for NOOP
  virtual MAIL_STORE_RESULT mailboxFlushBuffers(void) = 0;
  virtual MAIL_STORE_RESULT mailboxUpdateStats(NUMBER_SET *nowGone) = 0;
  // SYZYGY -- I also need a function that will cause the system to recalculate the
  // SYZYGY -- number of messages in the file and the next UID value, for those mailstores
  // SYZYGY -- where that is a lengthy operation so that I can recalculate it periodically
  // SYZYGY -- in a separate worker thread.  Or do I?  I need to try again when I'm farther
  // SYZYGY -- along and see what I need
  // If mailboxList fails, it returns nothing, which is fine for IMAP
  // If listAll is true, it will return all matching mailboxes, otherwise, it will return
  // the mailboxes using the IMAP LSUB semantics, which may not be what you expect.
  // You have been warned.
  virtual void mailboxList(const std::string &pattern, MAILBOX_LIST *result, bool listAll) = 0;
  virtual ~MailStore();
  std::string turnErrorCodeIntoString(MAIL_STORE_RESULT code);
  // This deletes a message in a mail box
  virtual MAIL_STORE_RESULT deleteMessage(const std::string &MailboxName, unsigned long uid) = 0;
  bool isMailboxOpen(void) { return NULL != m_openMailboxName; }
  std::string *mailboxName(void) const { return m_openMailboxName; }
  virtual MailMessage::MAIL_MESSAGE_RESULT messageData(MailMessage **message, unsigned long uid) = 0;
  virtual size_t bufferLength(unsigned long uid) = 0;
  virtual MAIL_STORE_RESULT openMessageFile(unsigned long uid) = 0;
  virtual size_t readMessage(char *buff, size_t offset, size_t length) = 0;
  virtual void closeMessageFile(void) = 0;
  virtual const SEARCH_RESULT *searchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate) = 0;
  virtual const std::string generateUrl(const std::string MailboxName) const = 0;
  virtual MailStore *clone(void) = 0;
  const MSN_TO_UID &uidGivenMsn(void) const {
    return m_uidGivenMsn;
  }
  virtual MAIL_STORE_RESULT lock(void) = 0;
  virtual MAIL_STORE_RESULT unlock(void) = 0;

protected:
  MSN_TO_UID m_uidGivenMsn;
  ImapSession *m_session;
  int m_errnoFromLibrary;
  std::string *m_openMailboxName;
};

#endif // _MAILSTORE_HPP_INCLUDED_
