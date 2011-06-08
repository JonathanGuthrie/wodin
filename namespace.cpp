#include <iostream>
#include <utility>

#include "namespace.hpp"

#include <algorithm>


pthread_mutex_t Namespace::m_mailboxMapMutex;
Namespace::MailboxMap Namespace::m_mailboxMap;
pthread_mutex_t Namespace::m_orphanMapMutex;
Namespace::MailStoreMap Namespace::m_orphanMap;

Namespace::Namespace(ImapSession *session) : MailStore(session) {
  m_defaultNamespace = NULL;
  m_openMailboxName = NULL;
  m_defaultType = BAD_NAMESPACE;
}

MailStore *Namespace::nameSpace(std::string &name) {
  MailStore *result = m_defaultNamespace;
  NamespaceMap::const_iterator i;

  for (i = m_namespaces.begin(); i != m_namespaces.end(); ++i) {
    if (0 == name.compare(0, i->first.size(), i->first)) {
      name.erase(0, i->first.size());
      result = i->second.store;
      break;
    }
  }
#if 0
  if (i != m_namespaces.end()) {
    std::cout << "The name is \"" << name << "\" and the key value is \"" << i->first << "\"" << std::endl;
  }
  else {
    std::cout << "Name \"" << name << "\" not found in any namespace" << std::endl;
  }
#endif // 0
  return result;
}

MailStore::MAIL_STORE_RESULT Namespace::createMailbox(const std::string &FullName) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  std::string changeableName = FullName;
  MailStore *store = nameSpace(changeableName);
  if (NULL != store) {
    result = store->createMailbox(changeableName);
  }

  return result;
}


// SYZYGY -- what are the consequences to deleting a mailbox that is open in another session?
// SYZYGY -- probably not good.  I should deal with it, shouldn't I?
// SYZYGY -- I should refuse to allow a client to delete a mailbox if any clients have it open
MailStore::MAIL_STORE_RESULT Namespace::deleteMailbox(const std::string &FullName) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  std::string changeableName = FullName;
  MailStore *store = nameSpace(changeableName);
  if (NULL != store) {
    result = store->deleteMailbox(changeableName);
  }
  else {
    if (NULL != m_defaultNamespace) {
      result = m_defaultNamespace->deleteMailbox(FullName);
    }
  }
  return result;
}

MailStore::MAIL_STORE_RESULT Namespace::renameMailbox(const std::string &SourceName, const std::string &DestinationName) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  std::string changeableSource = SourceName;
  std::string changeableDest = DestinationName;
  MailStore *sourceStore = nameSpace(changeableSource);
  MailStore *destStore = nameSpace(changeableDest);
  if (NULL != sourceStore) {
    if (sourceStore == destStore) {
      result = sourceStore->renameMailbox(SourceName, changeableDest);
    }
    else {
      result = NAMESPACES_DONT_MATCH;
    }
  }
  return result;
}


void Namespace::dump_message_session_info(const char *s, ExpungedMessageMap &m) {
  for (ExpungedMessageMap::iterator i = m.begin(); i != m.end(); ++i) {
    for (SessionList::iterator j = i->second.expungedSessions.begin(); j != i->second.expungedSessions.end(); ++j) {
      std::cout << s << " " << i->second.uid << " " << &*j << std::endl;
    }
  }
}

MailStore::MAIL_STORE_RESULT Namespace::mailboxClose() {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store) && (NULL != m_openMailboxName)) {
    // I reduce the reference count and remove this session from the list of those notified
    // about deleted messages wherever it appears.  I also check to see if there are any more
    // to purge from the back end and call the purge function if there are any additional
    // and then I call MailboxFlushBuffers
    std::string mailboxUrl = m_selectedMailbox->store->generateUrl(*m_openMailboxName);
    pthread_mutex_lock(&Namespace::m_mailboxMapMutex);
    MailboxMap::iterator found = m_mailboxMap.find(mailboxUrl);
    if (found != m_mailboxMap.end()) {
      --(found->second.refcount);
      if (1 > found->second.refcount) {
	// If the reference count is zero, then call mailboxClose and remove this from the mailboxMap and then
	// delete the mailstore
	result = m_selectedMailbox->store->mailboxClose();
	if (MailStore::CANNOT_COMPLETE_ACTION == result) {
	  // The mail box is locked, but the user wants it gone, so it is now an orphan
	  pthread_mutex_lock(&Namespace::m_orphanMapMutex);
	  m_orphanMap.insert(MailStoreMap::value_type(mailboxUrl, m_selectedMailbox->store));
	  pthread_mutex_unlock(&Namespace::m_orphanMapMutex);
	}
	else {
	  // if the close was successful, remove this from the mailboxMap and then delete the mailstore
	  pthread_mutex_destroy(&found->second.mutex);
	  delete m_selectedMailbox->store;
	}
	m_mailboxMap.erase(found);
	// no matter what, as far as the end user is concerned, everything worked just fine
	result = MailStore::SUCCESS;
      }
      else {
	// SYZYGY -- need to test the update of the list of sessions notified about messages from this mailstore
	dump_message_session_info("before_a", found->second.messages);
	for (ExpungedMessageMap::iterator i = found->second.messages.begin(); i != found->second.messages.end(); ++i) {
	  for (SessionList::iterator j = i->second.expungedSessions.begin(); j != i->second.expungedSessions.end(); ++j) {
	    if (m_session == *j) {
	      i->second.expungedSessions.erase(j);
	      i->second.expungedSessionCount--;
	      break;
	    }
	  }
	}
	dump_message_session_info("after_a", found->second.messages);
	// and flush the buffers
	m_selectedMailbox->store->mailboxFlushBuffers();
      }
    }
    pthread_mutex_unlock(&Namespace::m_mailboxMapMutex);

    // And I now have no selectedNamespace nor do I have an open mailbox
    m_selectedMailbox = NULL;
    m_openMailboxName = NULL;

  }
  return result;
}

void Namespace::mailboxList(const std::string &pattern, MAILBOX_LIST *result, bool listAll) {
  MailStore *store = NULL;
  std::string changeablePattern = pattern;
  store = nameSpace(changeablePattern);
  if (NULL != store) {
    store->mailboxList(changeablePattern, result, listAll);
  }
}

MailStore::MAIL_STORE_RESULT Namespace::subscribeMailbox(const std::string &MailboxName, bool isSubscribe) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  std::string changeableName = MailboxName;
  MailStore *store = nameSpace(changeableName);
  if (NULL != store) {
    result = store->subscribeMailbox(changeableName, isSubscribe);
  }
  return result;
}

MailStore::MAIL_STORE_RESULT Namespace::addMessageToMailbox(const std::string &MailboxName, uint8_t *data, size_t length,
							    DateTime &createTime, uint32_t messageFlags, size_t *newUid) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  std::string changeableName = MailboxName;
  MailStore *store = nameSpace(changeableName);
  if (NULL != store) {
    result = store->addMessageToMailbox(changeableName, data, length, createTime, messageFlags, newUid);
  }
  return result;
}

MailStore::MAIL_STORE_RESULT Namespace::appendDataToMessage(const std::string &MailboxName, size_t uid, uint8_t *data, size_t length) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  std::string changeableName = MailboxName;
  MailStore *store = nameSpace(changeableName);
  if (NULL != store) {
    result = store->appendDataToMessage(changeableName, uid, data, length);
  }
  return result;
}

MailStore::MAIL_STORE_RESULT Namespace::doneAppendingDataToMessage(const std::string &MailboxName, size_t uid) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  std::string changeableName = MailboxName;
  MailStore *store = nameSpace(changeableName);
  if (NULL != store) {
    result = store->doneAppendingDataToMessage(changeableName, uid);
  }
  return result;
}


// The next seven methods only have meaning if a mailbox has been opened, something I expect to be 
// enforced by the IMAP server logic because they're only meaningful in the selected state
unsigned Namespace::serialNumber() {
  unsigned result = 0;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->serialNumber();
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}

unsigned Namespace::uidValidityNumber() {
  unsigned result = 0;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->uidValidityNumber();
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}

// This needs to be called ONLY when the m_selectedMailbox->mutex has been locked
bool Namespace::addSession(int refCount, expunged_message_t &message) {
  bool result = false;
  SessionList::iterator i = std::find(message.expungedSessions.begin(), message.expungedSessions.end(), m_session);
  if (message.expungedSessions.end() == i) {
    message.expungedSessions.push_back(m_session);
    ++(message.expungedSessionCount);
    --m_mailboxMessageCount;
    if (refCount > message.expungedSessionCount) {
      m_selectedMailbox->store->expungeThisUid(message.uid);
      result = true;
    }
  }
  return result;
}

void Namespace::removeUid(unsigned long uid) {
  MSN_TO_UID::iterator i = std::find(m_uidGivenMsn.begin(), m_uidGivenMsn.end(), uid);
  if (m_uidGivenMsn.end() != i) {
    m_uidGivenMsn.erase(i);
  }
}


MailStore::MAIL_STORE_RESULT Namespace::mailboxOpen(const std::string &MailboxName, bool readWrite) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  std::string changeableName = MailboxName;
  MailStore *store = nameSpace(changeableName);
  if (NULL != store) {
    int refCount;

    std::string mailboxUrl = store->generateUrl(changeableName);
    pthread_mutex_lock(&Namespace::m_mailboxMapMutex);
    MailboxMap::iterator found = m_mailboxMap.find(mailboxUrl);
    if (found == m_mailboxMap.end()) {
      std::pair<MailboxMap::iterator, bool> insert_result;
      pthread_mutex_lock(&Namespace::m_orphanMapMutex);
      MailStoreMap::iterator foundOrphan = m_orphanMap.find(mailboxUrl);
      if (foundOrphan == m_orphanMap.end()) {
	MailStore *selectedNamespace = store->clone();
	// It's not an orphaned mail box, open it
	if (SUCCESS == (result = selectedNamespace->mailboxOpen(MailboxName, readWrite))) {
	  m_openMailboxName = selectedNamespace->mailboxName();

	  mailbox_t boxToInsert;
	  pthread_mutex_init(&boxToInsert.mutex, NULL);
	  boxToInsert.store = selectedNamespace;
	  refCount = boxToInsert.refcount = 1;
	  boxToInsert.messages.clear();
	  insert_result = m_mailboxMap.insert(MailboxMap::value_type(mailboxUrl, boxToInsert));
	  m_selectedMailbox = &(insert_result.first->second);
	}
	else {
	  delete selectedNamespace;
	  selectedNamespace = NULL;
	  m_openMailboxName = NULL;
	  m_selectedMailbox = NULL;
	}
      }
      else {
	// It's an orphaned mail box. I don't need to open it because it was never closed.

	mailbox_t boxToInsert;
	pthread_mutex_init(&boxToInsert.mutex, NULL);
	boxToInsert.store = foundOrphan->second;
	refCount = boxToInsert.refcount = 1;
	boxToInsert.messages.clear();
	insert_result = m_mailboxMap.insert(MailboxMap::value_type(mailboxUrl, boxToInsert));
	m_selectedMailbox = &(insert_result.first->second);
	m_openMailboxName = foundOrphan->second->mailboxName();
	m_orphanMap.erase(foundOrphan);
      }
      pthread_mutex_unlock(&Namespace::m_orphanMapMutex);
    }
    else {
      result = MailStore::SUCCESS;
      m_selectedMailbox = &(found->second);
      m_openMailboxName = found->second.store->mailboxName();
      ++(found->second.refcount);
      refCount = found->second.refcount;
    }
    if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
      pthread_mutex_lock(&m_selectedMailbox->mutex);
      // if selectedNamespace is not NULL then I need to generate the MSN/UID mapping for the
      // session and generate a message count
      m_mailboxMessageCount = m_selectedMailbox->store->mailboxMessageCount();
      m_uidGivenMsn.assign(m_selectedMailbox->store->uidGivenMsn().begin(), m_selectedMailbox->store->uidGivenMsn().end());
      for (ExpungedMessageMap::iterator i = m_selectedMailbox->messages.begin(); i!= m_selectedMailbox->messages.end(); ++i) {
	--m_mailboxMessageCount;
	removeUid(i->second.uid);
	if (addSession(refCount, i->second)) {
	  m_selectedMailbox->messages.erase(i);
	}
      }
      pthread_mutex_unlock(&m_selectedMailbox->mutex);
    }
    pthread_mutex_unlock(&Namespace::m_mailboxMapMutex);
  }
  return result;
}

MailStore::MAIL_STORE_RESULT Namespace::listDeletedMessages(NUMBER_SET *nowGone) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    // This gets the messages that some other session has expunged and this one hasn't
    mailboxUpdateStatsInternal(nowGone);
    // This gets the messages that no sessions know about but which need to be expunged
    result = m_selectedMailbox->store->listDeletedMessages(nowGone);
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}


MailStore::MAIL_STORE_RESULT Namespace::expungeThisUid(unsigned long uid) {
  // SYZYGY -- find the uid in the map
  // SYZYGY -- then add this session to the list of those who have seen it as deleted
  // SYZYGY -- if addSession returns true, then actually expunge it and pull it out of the list of messages waiting to be expunged
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    dump_message_session_info("before_b", m_selectedMailbox->messages);
    ExpungedMessageMap::iterator found = m_selectedMailbox->messages.find(uid);
    if (m_selectedMailbox->messages.end() != found) {
      if (addSession(m_selectedMailbox->refcount, found->second)) {
	result = m_selectedMailbox->store->expungeThisUid(uid);
      }
      m_selectedMailbox->messages.erase(found);
    }
    else {
      // I have to add it, if the refcount is greater than one
      // if it's not greater than one, I can just purge it, no harm done
      if (1 < m_selectedMailbox->refcount) {
	std::pair<ExpungedMessageMap::iterator, bool> insert_result;
	expunged_message_t message;
	message.uid = uid;
	message.expungedSessionCount  = 0;
	insert_result = m_selectedMailbox->messages.insert(ExpungedMessageMap::value_type(uid, message));
	addSession(m_selectedMailbox->refcount, insert_result.first->second);
      }
      else {
	--m_mailboxMessageCount;
	result = m_selectedMailbox->store->expungeThisUid(uid);
      }
    }
    dump_message_session_info("after_b", m_selectedMailbox->messages);
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  removeUid(uid);
  return result;
}


MailStore::MAIL_STORE_RESULT Namespace::getMailboxCounts(const std::string &MailboxName, uint32_t which, unsigned &messageCount,
							 unsigned &recentCount, unsigned &uidNext, unsigned &uidValidity,
							 unsigned &firstUnseen) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  std::string changeableName = MailboxName;
  MailStore *store = nameSpace(changeableName);
  if (NULL != store) {
    result = store->getMailboxCounts(MailboxName, which, messageCount, recentCount, uidNext, uidValidity, firstUnseen);
  }
  return result;
}

unsigned Namespace::mailboxMessageCount() {
  return m_mailboxMessageCount;
}

// SYZYGY -- Recent is now more complicted GRRR!
unsigned Namespace::mailboxRecentCount() {
  unsigned result = 0;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->mailboxRecentCount();
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}

unsigned Namespace::mailboxFirstUnseen() {
  unsigned result = 0;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->mailboxFirstUnseen();
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}

const DateTime &Namespace::messageInternalDate(const unsigned long uid) {
  // SYZYGY -- I need something like a void cast to DateTime that returns an error
  static DateTime result;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->messageInternalDate(uid);
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}

NUMBER_LIST Namespace::mailboxMsnToUid(const NUMBER_LIST &msns) {
  NUMBER_LIST result;
  for (NUMBER_LIST::const_iterator i=msns.begin(); i!=msns.end(); ++i) {
    result.push_back(mailboxMsnToUid(*i));
  }
  return result;
}

unsigned long Namespace::mailboxMsnToUid(const unsigned long msn) {
  unsigned long result = 0;
  if (msn <= m_uidGivenMsn.size()) {
    result = m_uidGivenMsn[msn-1];
  }
  return result;
}

NUMBER_LIST Namespace::mailboxUidToMsn(const NUMBER_LIST &uids) {
  NUMBER_LIST result;
  for (NUMBER_LIST::const_iterator i=uids.begin(); i!=uids.end(); ++i) {
    result.push_back(mailboxUidToMsn(*i));
  }
}

unsigned long Namespace::mailboxUidToMsn(unsigned long uid) {
  unsigned long result = 0;

  MSN_TO_UID::const_iterator i = find(m_uidGivenMsn.begin(), m_uidGivenMsn.end(), uid);
  if (i != m_uidGivenMsn.end()) {
    result = (i - m_uidGivenMsn.begin()) + 1;
  }
  return result;
}

MailStore::MAIL_STORE_RESULT Namespace::messageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->messageUpdateFlags(uid, andMask, orMask, flags);
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}

std::string Namespace::mailboxUserPath() const {
  std::string result = "";
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->mailboxUserPath();
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}

MailStore::MAIL_STORE_RESULT Namespace::mailboxFlushBuffers(void) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->mailboxFlushBuffers();
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}

// This does the work from MailboxUpdateStats and also some of the work for ListDeletedMessages
// I can't just call MailboxUpdateStats from ListDeletedMessages because then I get a deadlock
// because both of them MUST lock the m_selectedMailboxMutex but both of them can call this
// function because this function doesn't lock anything.
MailStore::MAIL_STORE_RESULT Namespace::mailboxUpdateStatsInternal(NUMBER_SET *nowGone) {
  MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
  for (ExpungedMessageMap::iterator i = m_selectedMailbox->messages.begin(); i!= m_selectedMailbox->messages.end(); ++i) {
    bool found = false;

    for (SessionList::iterator j = i->second.expungedSessions.begin(); j != i->second.expungedSessions.end(); ++j) {
      if (m_session == *j) {
	found = true;
	break;
      }
    }
    if (!found) {
      nowGone->insert(nowGone->begin(), i->first);
    }
  }
}

MailStore::MAIL_STORE_RESULT Namespace::mailboxUpdateStats(NUMBER_SET *nowGone) {
  MailStore::MAIL_STORE_RESULT result = MailStore::GENERAL_FAILURE;
  if (NULL != m_selectedMailbox) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = mailboxUpdateStatsInternal(nowGone);
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  // Attempt to close any orphaned mail stores
  pthread_mutex_lock(&Namespace::m_orphanMapMutex);
  for (MailStoreMap::iterator i = m_orphanMap.begin(); i!= m_orphanMap.end(); ++i) {
    if (MailStore::SUCCESS == i->second->mailboxClose()) {
      m_orphanMap.erase(i);
    }
  }
  pthread_mutex_unlock(&Namespace::m_orphanMapMutex);
  return result;
}

Namespace::~Namespace() {
  m_openMailboxName = NULL;
  for (NamespaceMap::iterator i = m_namespaces.begin(); i != m_namespaces.end(); ++i) {
    delete i->second.store;
  }
  m_namespaces.clear();
  if (NULL != m_defaultNamespace) {
    delete m_defaultNamespace;
  }
}

void Namespace::addNamespace(NAMESPACE_TYPES type, const std::string &name, MailStore *handler, char separator) {
  if ("" == name) {
    if (NULL != m_defaultNamespace) {
      delete m_defaultNamespace;
    }
    m_defaultType = type;
    m_defaultNamespace = handler;
    m_defaultSeparator = separator;
  }
  else {
    namespace_t namespaceToInsert;

    namespaceToInsert.store = handler;
    namespaceToInsert.separator = separator;
    namespaceToInsert.type = type;
    m_namespaces.insert(NamespaceMap::value_type(name, namespaceToInsert));
  }
}

// Do I really want to return the data in the form needed by IMAP, or
// do I want to abstract it, somehow.  It's abstracted....for now
const std::string Namespace::listNamespaces(void) const {
  std::string result = "";
  bool nilFlag;

  NAMESPACE_TYPES t = PERSONAL;
  do {
    nilFlag = true;
    if (t == m_defaultType) {
      nilFlag = false;
      result += "((\"\" ";
      if ('\0' != m_defaultSeparator) {
	result += "\"";
	result += m_defaultSeparator;
	result += "\"";
      }
      result += ")";
    }
    for (NamespaceMap::const_iterator i = m_namespaces.begin(); i != m_namespaces.end(); ++i) {
      if (t == i->second.type) {
	if (nilFlag) {
	  result += "(";	    
	}
	nilFlag = false;
	result += "(\"";
	result += i->first.c_str();
	result += "\" ";
	if ('\0' != i->second.separator) {
	  result += "\"";
	  result += i->second.separator;
	  result += "\"";
	}
	else {
	  result += "NIL";
	}
	result += ")";
      }
    }
    if (nilFlag) {
      result += "NIL";
    }
    else {
      result += ")";
    }
    if (SHARED != t) {
      result += " ";
    }
    t = PERSONAL == t ? OTHERS : (OTHERS == t ? SHARED : BAD_NAMESPACE);
  } while (t < BAD_NAMESPACE);
  return result;
}

MailStore::MAIL_STORE_RESULT Namespace::deleteMessage(const std::string &MailboxName, unsigned long uid) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  std::string changeableName = MailboxName;
  MailStore *store = nameSpace(changeableName);
  if (NULL != store) {
    result = store->deleteMessage(changeableName, uid);
  }
  return result;
}

MailMessage::MAIL_MESSAGE_RESULT Namespace::messageData(MailMessage **message, unsigned long uid) {
  MailMessage::MAIL_MESSAGE_RESULT result = MailMessage::GENERAL_FAILURE;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->messageData(message, uid);
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}

MailStore::MAIL_STORE_RESULT Namespace::openMessageFile(unsigned long uid) {
  MailStore::MAIL_STORE_RESULT result = MailStore::GENERAL_FAILURE;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->openMessageFile(uid);
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}

size_t Namespace::bufferLength(unsigned long uid) {
  size_t result = 0;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->bufferLength(uid);
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}

size_t Namespace::readMessage(char *buffer, size_t offset, size_t length) {
  size_t result = 0;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->readMessage(buffer, offset, length);
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}

void Namespace::closeMessageFile(void) {
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    m_selectedMailbox->store->closeMessageFile();
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
}


const SEARCH_RESULT *Namespace::searchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate) {
  const SEARCH_RESULT *result = NULL;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->searchMetaData(xorMask, andMask, smallestSize, largestSize, beginInternalDate, endInternalDate);
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}



const std::string Namespace::generateUrl(const std::string MailboxName) const {
  std::string result;
  result = "namespace:///" + MailboxName;
  return result;
}


Namespace *Namespace::clone(void) {
  return new Namespace(m_session);
}


MailStore::MAIL_STORE_RESULT Namespace::lock(void) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->lock();
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}


MailStore::MAIL_STORE_RESULT Namespace::unlock(void) {
  MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
  if ((NULL != m_selectedMailbox) && (NULL != m_selectedMailbox->store)) {
    pthread_mutex_lock(&m_selectedMailbox->mutex);
    result = m_selectedMailbox->store->unlock();
    pthread_mutex_unlock(&m_selectedMailbox->mutex);
  }
  return result;
}


size_t Namespace::orphanCount(void) {
  size_t result;
  pthread_mutex_lock(&Namespace::m_orphanMapMutex);
  result = m_orphanMap.size();
  pthread_mutex_unlock(&Namespace::m_orphanMapMutex);
  return result;
}
