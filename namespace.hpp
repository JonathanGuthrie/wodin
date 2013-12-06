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

#if !defined(_NAMESPACE_HPP_INCLUDED_)
#define _NAMESPACE_HPP_INCLUDED_

#include <map>
#include <list>

#include <pthread.h>

#include "mailstore.hpp"

class Namespace : public MailStore {
public:
    typedef enum {
	PERSONAL,
	OTHERS,
	SHARED,
	BAD_NAMESPACE
    } NAMESPACE_TYPES;
	
    typedef struct {
	MailStore *store;
	char separator;
	NAMESPACE_TYPES type;
    } namespace_t;

    typedef std::map<std::string, namespace_t> NamespaceMap;

    Namespace(ImapSession *session);
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

    virtual NUMBER_LIST mailboxMsnToUid(const NUMBER_LIST &msns);
    virtual unsigned long mailboxMsnToUid(unsigned long msn);
    virtual NUMBER_LIST mailboxUidToMsn(const NUMBER_LIST &uids);
    virtual unsigned long mailboxUidToMsn(unsigned long uid);

    // This updates the flags associated with the email message
    // of 'orig' is the original flag set, then the final flag set is 
    // orMask | (andMask & orig)
    // The final flag set is returned in flags
    virtual MailStore::MAIL_STORE_RESULT messageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags);

    virtual std::string mailboxUserPath() const ;
    virtual MailStore::MAIL_STORE_RESULT mailboxFlushBuffers(void);
    virtual MailStore::MAIL_STORE_RESULT mailboxUpdateStats(NUMBER_SET *nowGone);
    virtual void mailboxList(const std::string &pattern, MAILBOX_LIST *result, bool listAll);
    virtual ~Namespace();
    void addNamespace(NAMESPACE_TYPES type, const std::string &name, MailStore *handler, char separator = '\0');
    const std::string listNamespaces(void) const;
    // This deletes a message in a mail box
    virtual MailStore::MAIL_STORE_RESULT deleteMessage(const std::string &MailboxName, unsigned long uid);
    virtual MailMessage::MAIL_MESSAGE_RESULT messageData(MailMessage **message, unsigned long uid);
    virtual size_t bufferLength(unsigned long uid);
    virtual MailStore::MAIL_STORE_RESULT openMessageFile(unsigned long uid);
    virtual size_t readMessage(char *buff, size_t offset, size_t length);
    virtual void closeMessageFile(void);
    virtual const SEARCH_RESULT *searchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate);
    virtual const std::string generateUrl(const std::string MailboxName) const;
    virtual Namespace *clone(void);
    virtual MailStore::MAIL_STORE_RESULT lock(void);
    virtual MailStore::MAIL_STORE_RESULT unlock(void);
    MailStore::MAIL_STORE_RESULT lock(const std::string &mailboxName);
    MailStore::MAIL_STORE_RESULT unlock(const std::string &mailboxName);

    static void runtime_init(void) {
	pthread_mutex_init(&m_mailboxMapMutex, NULL);
	m_mailboxMap.clear();
    }

    size_t orphanCount(void);

private:
    // SYZYGY -- each item in the MailboxMap class must have a mail store that is talking to the
    // SYZYGY -- mail store for that mailbox, a ref count so that it knows when to clean up, and
    // SYZYGY -- a list of messages that are in the process of being expunged.  Each message
    // SYZYGY -- consists of a message's UID, a count of how many sessions have expunged that
    // SYZYGY -- message, and a list of all the sessions that have expunged it.
    typedef std::list<ImapSession *> SessionList;
    typedef struct {
	unsigned long uid;
	unsigned expungedSessionCount;
	SessionList expungedSessions;
    } expunged_message_t;
    typedef std::map<unsigned, expunged_message_t> ExpungedMessageMap;
    // SYZYGY -- This needs a readonly flag because otherwise you get whatever state (read/write or
    // SYZYGY -- read only) that the first one opened has.
    typedef struct {
	MailStore *store;
	ExpungedMessageMap messages;
	int refcount;
	pthread_mutex_t mutex;
    } mailbox_t;
    typedef std::map<std::string, mailbox_t> MailboxMap;
    static MailboxMap m_mailboxMap;
    static pthread_mutex_t m_mailboxMapMutex;
    typedef std::map<std::string, MailStore *> MailStoreMap;
    static MailStoreMap m_orphanMap;
    static pthread_mutex_t m_orphanMapMutex;

    mailbox_t *m_selectedMailbox;

    MailStore *nameSpace(std::string &name);
    NamespaceMap m_namespaces;
    NAMESPACE_TYPES m_defaultType;
    MailStore *m_defaultNamespace;
    char m_defaultSeparator;
    void removeUid(unsigned long uid);
    bool addSession(size_t refCount, expunged_message_t &message);
    unsigned m_mailboxMessageCount;
    void dump_message_session_info(const char *s, ExpungedMessageMap &m);
    MailStore::MAIL_STORE_RESULT mailboxUpdateStatsInternal(NUMBER_SET *uidsToBeExpunged);
};

#endif // _NAMESPACE_HPP_INCLUDED_
