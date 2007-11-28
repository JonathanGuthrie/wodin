#include <iostream>
#include <utility>

#include "namespace.hpp"


pthread_mutex_t Namespace::m_mailboxMapMutex;
Namespace::MailboxMap Namespace::m_mailboxMap;

Namespace::Namespace(ImapSession *session) : MailStore(session) {
    defaultNamespace = NULL;
    selectedNamespace = NULL;
    m_openMailboxName = NULL;
    defaultType = BAD_NAMESPACE;
}

MailStore *Namespace::getNameSpace(const std::string &name) {
    MailStore *result = defaultNamespace;
    NamespaceMap::const_iterator i;

    for (i = namespaces.begin(); i != namespaces.end(); ++i) {
	if (0 == name.compare(0, i->first.size(), i->first)) {
	    result = i->second.store;
	    break;
	}
    }
#if 0
    if (i != namespaces.end()) {
	std::cout << "The name is \"" << name << "\" and the key value is \"" << i->first << "\"" << std::endl;
    }
    else {
	std::cout << "Name \"" << name << "\" not found in any namespace" << std::endl;
    }
#endif // 0
    return result;
}

MailStore::MAIL_STORE_RESULT Namespace::CreateMailbox(const std::string &FullName) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    MailStore *store = getNameSpace(FullName);
    if (NULL != store) {
	// SYZYGY -- I need to strip off the namespace, I think
	result = store->CreateMailbox(FullName);
    }

    return result;
}


// SYZYGY -- what are the consequences to deleting a mailbox that is open in another session?
// SYZYGY -- probably not good.  I should deal with it, shouldn't I?
MailStore::MAIL_STORE_RESULT Namespace::DeleteMailbox(const std::string &FullName) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    MailStore *store = getNameSpace(FullName);
    if (NULL != store) {
	// SYZYGY -- I need to strip off the namespace, I think
	result = store->DeleteMailbox(FullName);
    }
    else {
	if (NULL != defaultNamespace) {
	    result = defaultNamespace->DeleteMailbox(FullName);
	}
    }
    return result;
}

MailStore::MAIL_STORE_RESULT Namespace::RenameMailbox(const std::string &SourceName, const std::string &DestinationName) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    // SYZYGY -- I need to find out about how renaming works WRT name spaces
    MailStore *store = getNameSpace(SourceName);
    if (NULL != store) {
	// SYZYGY -- I need to strip off the namespace, I think
	result = store->RenameMailbox(SourceName, DestinationName);
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

// SYZYGY -- this now depends upon the reference count
MailStore::MAIL_STORE_RESULT Namespace::MailboxClose() {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    if (NULL != selectedNamespace) {
	// I reduce the reference count and remove this session from the list of those notified
	// about deleted messages wherever it appears.  I also check to see if there are any more
	// to purge from the back end and call the purge function if there are any additional
	// and then I call MailboxFlushBuffers
	std::string mailboxUrl = selectedNamespace->GenerateUrl(*m_openMailboxName);
	pthread_mutex_lock(&Namespace::m_mailboxMapMutex);
	MailboxMap::iterator found = m_mailboxMap.find(mailboxUrl);
	if (found != m_mailboxMap.end()) {
	    --(found->second.refcount);
	    if (1 > found->second.refcount) {
		// If the reference count is zero, then call mailboxClose and remove this from the mailboxMap and then
		// delete the mailstore
		result = selectedNamespace->MailboxClose();
		m_mailboxMap.erase(found);
		delete selectedNamespace;
	    }
	    else {
		// SYZYGY -- need to test the update of the list of sessions notified about messages from this mailstore
		dump_message_session_info("before", found->second.messages);
		for (ExpungedMessageMap::iterator i = found->second.messages.begin(); i != found->second.messages.end(); ++i) {
		    for (SessionList::iterator j = i->second.expungedSessions.begin(); j != i->second.expungedSessions.end(); ++j) {
			if (m_session == *j) {
			    i->second.expungedSessions.erase(j);
			    break;
			}
		    }
		}
		dump_message_session_info("after", found->second.messages);
		// and flush the buffers
		selectedNamespace->MailboxFlushBuffers();
	    }
	}
	pthread_mutex_unlock(&Namespace::m_mailboxMapMutex);

	// And I now have no selectedNamespace nor do I have an open mailbox
	selectedNamespace = NULL;
	m_openMailboxName = NULL;

    }
    return result;
}

void Namespace::BuildMailboxList(const std::string &pattern, MAILBOX_LIST *result, bool listAll) {
    MailStore *store = NULL;
    store = getNameSpace(pattern);
    if (NULL != store) {
	// SYZYGY -- I need to strip off the namespace, I think
	store->BuildMailboxList(pattern, result, listAll);
    }
}

MailStore::MAIL_STORE_RESULT Namespace::SubscribeMailbox(const std::string &MailboxName, bool isSubscribe) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    MailStore *store = getNameSpace(MailboxName);
    if (NULL != store) {
	// SYZYGY -- I need to strip off the namespace, I think
	result = store->SubscribeMailbox(MailboxName, isSubscribe);
    }
    return result;
}

MailStore::MAIL_STORE_RESULT Namespace::AddMessageToMailbox(const std::string &MailboxName, uint8_t *data, size_t length,
						 DateTime &createTime, uint32_t messageFlags, size_t *newUid) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    MailStore *store = getNameSpace(MailboxName);
    if (NULL != store) {
	// SYZYGY -- I need to strip off the namespace, I think
	result = store->AddMessageToMailbox(MailboxName, data, length, createTime, messageFlags, newUid);
    }
    return result;
}

MailStore::MAIL_STORE_RESULT Namespace::AppendDataToMessage(const std::string &MailboxName, size_t uid, uint8_t *data, size_t length) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    MailStore *store = getNameSpace(MailboxName);
    if (NULL != store) {
	// SYZYGY -- I need to strip off the namespace, I think
	result = store->AppendDataToMessage(MailboxName, uid, data, length);
    }
    return result;
}

MailStore::MAIL_STORE_RESULT Namespace::DoneAppendingDataToMessage(const std::string &MailboxName, size_t uid) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    MailStore *store = getNameSpace(MailboxName);
    if (NULL != store) {
	// SYZYGY -- I need to strip off the namespace, I think
	result = store->DoneAppendingDataToMessage(MailboxName, uid);
    }
    return result;
}


// The next seven methods only have meaning if a mailbox has been opened, something I expect to be 
// enforced by the IMAP server logic because they're only meaningful in the selected state
unsigned Namespace::GetSerialNumber() {
    unsigned result = 0;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->GetSerialNumber();
    }
    return result;
}

unsigned Namespace::GetUidValidityNumber() {
    unsigned result = 0;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->GetUidValidityNumber();
    }
    return result;
}

bool Namespace::addSession(int refCount, expunged_message_t &message) {
    bool result = false;
    SessionList::iterator i = std::find(message.expungedSessions.begin(), message.expungedSessions.end(), m_session);
    if (message.expungedSessions.end() == i) {
	message.expungedSessions.push_back(m_session);
	++(message.expungedSessionCount);
	--m_mailboxMessageCount;
	if (refCount > message.expungedSessionCount) {
	    selectedNamespace->ExpungeThisUid(message.uid);
	    true;
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


MailStore::MAIL_STORE_RESULT Namespace::MailboxOpen(const std::string &MailboxName, bool readWrite) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    MailStore *store = getNameSpace(MailboxName);
    if (NULL != store) {
	int refCount;

	// SYZYGY -- I need to strip off the namespace, I think
	std::string mailboxUrl = store->GenerateUrl(MailboxName);
	pthread_mutex_lock(&Namespace::m_mailboxMapMutex);
	MailboxMap::iterator found = m_mailboxMap.find(mailboxUrl);
	if (found == m_mailboxMap.end()) {
	    selectedNamespace = store->clone();
	    if (SUCCESS == (result = selectedNamespace->MailboxOpen(MailboxName, readWrite))) {
		std::pair<MailboxMap::iterator, bool> insert_result;
		m_openMailboxName = selectedNamespace->GetMailboxName();

		mailbox_t boxToInsert;
		boxToInsert.store = selectedNamespace;
		refCount = boxToInsert.refcount = 1;
		boxToInsert.messages.clear();
		insert_result = m_mailboxMap.insert(MailboxMap::value_type(mailboxUrl, boxToInsert));
		m_openMailbox = &(insert_result.first->second);
	    }
	    else {
		delete selectedNamespace;
		selectedNamespace = NULL;
		m_openMailboxName = NULL;
		m_openMailbox = NULL;
	    }
	}
	else {
	    result = MailStore::SUCCESS;
	    m_openMailbox = &(found->second);
	    selectedNamespace = found->second.store;
	    m_openMailboxName = found->second.store->GetMailboxName();
	    ++(found->second.refcount);
	    refCount = found->second.refcount;
	}
	if (NULL != selectedNamespace) {
	    // SYZYGY -- if selectedNamespace is not NULL then I need to generate the MSN/UID mapping for the
	    // SYZYGY -- session and generate a message count
	    m_mailboxMessageCount = selectedNamespace->MailboxMessageCount();
	    m_uidGivenMsn.assign(selectedNamespace->uidGivenMsn().begin(), selectedNamespace->uidGivenMsn().end());
	    if (NULL != m_openMailbox) {
		for (ExpungedMessageMap::iterator i = m_openMailbox->messages.begin(); i!= m_openMailbox->messages.end(); ++i) {
		    --m_mailboxMessageCount;
		    removeUid(i->second.uid);
		    if (addSession(refCount, i->second)) {
			m_openMailbox->messages.erase(i);
		    }
		}
	    }
	}
	pthread_mutex_unlock(&Namespace::m_mailboxMapMutex);
    }
    return result;
}

MailStore::MAIL_STORE_RESULT Namespace::ListDeletedMessages(NUMBER_LIST *nowGone) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->ListDeletedMessages(nowGone);
    }
    return result;
}


MailStore::MAIL_STORE_RESULT Namespace::ExpungeThisUid(unsigned long uid) {
    // SYZYGY -- find the uid in the map
    // SYZYGY -- then add this session to the list of those who have seen it as deleted
    // SYZYGY -- if addSession returns true, then actually expunge it and pull it out of the list of messages waiting to be expunged
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    pthread_mutex_lock(&Namespace::m_mailboxMapMutex);
    dump_message_session_info("before", m_openMailbox->messages);
    ExpungedMessageMap::iterator found = m_openMailbox->messages.find(uid);
    if (m_openMailbox->messages.end() != found) {
	if (addSession(m_openMailbox->refcount, found->second)) {
	    if (NULL != selectedNamespace) {
		result = selectedNamespace->ExpungeThisUid(uid);
	    }
	    m_openMailbox->messages.erase(found);
	}
    }
    else {
	// I have to add it, if the refcount is greater than one
	// if it's not greater than one, I can just purge it, no harm done
	if (1 < m_openMailbox->refcount) {
	    std::pair<ExpungedMessageMap::iterator, bool> insert_result;
	    expunged_message_t message;
	    message.uid = uid;
	    message.expungedSessionCount  = 0;
	    insert_result = m_openMailbox->messages.insert(ExpungedMessageMap::value_type(uid, message));
	    addSession(m_openMailbox->refcount, insert_result.first->second);
	}
	else {
	    if (NULL != selectedNamespace) {
		result = selectedNamespace->ExpungeThisUid(uid);
	    }
	}
    }
    // SYZYGY -- working here
    // SYZYGY -- I need to determine the reference count
    // SYZYGY -- and insert the current session, if it's not already in
    // SYZYGY -- and
    dump_message_session_info("after", m_openMailbox->messages);
    pthread_mutex_unlock(&Namespace::m_mailboxMapMutex);
    removeUid(uid);
    --m_mailboxMessageCount;
    return result;
}


MailStore::MAIL_STORE_RESULT Namespace::GetMailboxCounts(const std::string &MailboxName, uint32_t which, unsigned &messageCount,
							 unsigned &recentCount, unsigned &uidNext, unsigned &uidValidity,
							 unsigned &firstUnseen) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    MailStore *store = getNameSpace(MailboxName);
    if (NULL != store) {
	result = store->GetMailboxCounts(MailboxName, which, messageCount, recentCount, uidNext, uidValidity, firstUnseen);
    }
    return result;
}

unsigned Namespace::MailboxMessageCount() {
    return m_mailboxMessageCount;
}

// SYZYGY -- Recent is now more complicted GRRR!
unsigned Namespace::MailboxRecentCount() {
    unsigned result = 0;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->MailboxRecentCount();
    }
    return result;
}

unsigned Namespace::MailboxFirstUnseen() {
    unsigned result = 0;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->MailboxFirstUnseen();
    }
    return result;
}

const DateTime &Namespace::MessageInternalDate(const unsigned long uid) {
    // SYZYGY -- I need something like a void cast to DateTime that returns an error
    static DateTime result;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->MessageInternalDate(uid);
    }
    return result;
}

NUMBER_LIST Namespace::MailboxMsnToUid(const NUMBER_LIST &msns) {
    NUMBER_LIST result;
    for (NUMBER_LIST::const_iterator i=msns.begin(); i!=msns.end(); ++i) {
	result.push_back(MailboxMsnToUid(*i));
    }
    return result;
}

unsigned long Namespace::MailboxMsnToUid(const unsigned long msn) {
    unsigned long result = 0;
    if (msn <= m_uidGivenMsn.size()) {
	result = m_uidGivenMsn[msn-1];
    }
    return result;
}

NUMBER_LIST Namespace::MailboxUidToMsn(const NUMBER_LIST &uids) {
    NUMBER_LIST result;
    for (NUMBER_LIST::const_iterator i=uids.begin(); i!=uids.end(); ++i) {
	result.push_back(MailboxUidToMsn(*i));
    }
}

unsigned long Namespace::MailboxUidToMsn(unsigned long uid) {
    unsigned long result = 0;

    MSN_TO_UID::const_iterator i = find(m_uidGivenMsn.begin(), m_uidGivenMsn.end(), uid);
    if (i != m_uidGivenMsn.end()) {
	result = (i - m_uidGivenMsn.begin()) + 1;
    }
    return result;
}

MailStore::MAIL_STORE_RESULT Namespace::MessageUpdateFlags(unsigned long uid, uint32_t andMask, uint32_t orMask, uint32_t &flags) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->MessageUpdateFlags(uid, andMask, orMask, flags);
    }
    return result;
}

std::string Namespace::GetMailboxUserPath() const {
    std::string result = "";
    if (NULL != selectedNamespace) {
	result = selectedNamespace->GetMailboxUserPath();
    }
    return result;
}

MailStore::MAIL_STORE_RESULT Namespace::MailboxFlushBuffers(void) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->MailboxFlushBuffers();
    }
    return result;
}

MailStore::MAIL_STORE_RESULT Namespace::MailboxUpdateStats(NUMBER_LIST *nowGone) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->MailboxUpdateStats(nowGone);
    }
    return result;
}

Namespace::~Namespace() {
    m_openMailboxName = NULL;
    for (NamespaceMap::iterator i = namespaces.begin(); i != namespaces.end(); ++i) {
	delete i->second.store;
    }
    namespaces.clear();
    if (NULL != defaultNamespace) {
	delete defaultNamespace;
    }
}

void Namespace::AddNamespace(NAMESPACE_TYPES type, const std::string &name, MailStore *handler, char separator) {
    if ("" == name) {
	if (NULL != defaultNamespace) {
	    delete defaultNamespace;
	}
	defaultType = type;
	defaultNamespace = handler;
	defaultSeparator = separator;
    }
    else {
	namespace_t namespaceToInsert;

	namespaceToInsert.store = handler;
	namespaceToInsert.separator = separator;
	namespaceToInsert.type = type;
	namespaces.insert(NamespaceMap::value_type(name, namespaceToInsert));
    }
}

// Do I really want to return the data in the form needed by IMAP, or
// do I want to abstract it, somehow.  It's abstracted....for now
const std::string Namespace::ListNamespaces(void) const {
    std::string result = "";
    bool nilFlag;

    NAMESPACE_TYPES t = PERSONAL;
    do {
	nilFlag = true;
	if (t == defaultType) {
	    nilFlag = false;
	    result += "((\"\" ";
	    if ('\0' != defaultSeparator) {
		result += "\"";
		result += defaultSeparator;
		result += "\"";
	    }
	    result += ")";
	}
	for (NamespaceMap::const_iterator i = namespaces.begin(); i != namespaces.end(); ++i) {
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

MailStore::MAIL_STORE_RESULT Namespace::DeleteMessage(const std::string &MailboxName, unsigned long uid) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    MailStore *store = getNameSpace(MailboxName);
    if (NULL != store) {
	// SYZYGY -- I need to strip off the namespace, I think
	result = store->DeleteMessage(MailboxName, uid);
    }
    return result;
}

MailMessage::MAIL_MESSAGE_RESULT Namespace::GetMessageData(MailMessage **message, unsigned long uid) {
    MailMessage::MAIL_MESSAGE_RESULT result = MailMessage::GENERAL_FAILURE;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->GetMessageData(message, uid);
    }
    return result;
}

MailStore::MAIL_STORE_RESULT Namespace::OpenMessageFile(unsigned long uid) {
    MailStore::MAIL_STORE_RESULT result = MailStore::GENERAL_FAILURE;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->OpenMessageFile(uid);
    }
    return result;
}

size_t Namespace::GetBufferLength(unsigned long uid) {
    size_t result = 0;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->GetBufferLength(uid);
    }
    return result;
}

size_t Namespace::ReadMessage(char *buffer, size_t offset, size_t length) {
    size_t result = 0;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->ReadMessage(buffer, offset, length);
    }
    return result;
}

void Namespace::CloseMessageFile(void) {
    if (NULL != selectedNamespace) {
	selectedNamespace->CloseMessageFile();
    }
}


const SEARCH_RESULT *Namespace::SearchMetaData(uint32_t xorMask, uint32_t andMask, size_t smallestSize, size_t largestSize, DateTime *beginInternalDate, DateTime *endInternalDate) {
    const SEARCH_RESULT *result = NULL;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->SearchMetaData(xorMask, andMask, smallestSize, largestSize, beginInternalDate, endInternalDate);
    }
    return result;
}



const std::string Namespace::GenerateUrl(const std::string MailboxName) const {
    std::string result;
    result = "namespace:///" + MailboxName;
    return result;
}


Namespace *Namespace::clone(void) {
    return new Namespace(m_session);
}
