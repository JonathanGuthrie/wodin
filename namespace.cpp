#include <iostream>

#include "namespace.hpp"

// SYZYGY -- working here:  I need to define a map of the namespaces.
// SYZYGY -- that needs to include all the data required by ListNameSpaces

Namespace::Namespace(ImapSession *session) : MailStore(session) {
    defaultNamespace = NULL;
    selectedNamespace = NULL;
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

MailStore::MAIL_STORE_RESULT Namespace::MailboxClose() {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->MailboxClose();
	selectedNamespace = NULL;
    }
    return result;
}

void Namespace::BuildMailboxList(const char *ref, const char *pattern, MAILBOX_LIST *result, bool listAll) {
    MailStore *store = NULL;
    if (0 != strlen(ref)) {
	store = getNameSpace(ref);
    }
    else {
	store = getNameSpace(pattern);
    }
    if (NULL != store) {
	// SYZYGY -- I need to strip off the namespace, I think
	store->BuildMailboxList(ref, pattern, result, listAll);
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

// The next seven methods only have meaning if a mailbox has been opened, something I expect to be 
// enforced by the IMAP server logic because they're only meaningful in the selected state
unsigned Namespace::GetSerialNumber() {
    unsigned result = 0;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->GetSerialNumber();
    }
    return result;
}

unsigned Namespace::GetNextSerialNumber() {
    unsigned result = 0;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->GetNextSerialNumber();
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

MailStore::MAIL_STORE_RESULT Namespace::MailboxOpen(const std::string &MailboxName, bool readWrite) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    MailStore *store = getNameSpace(MailboxName);
    if (NULL != store) {
	// SYZYGY -- I need to strip off the namespace, I think
	if (SUCCESS == (result = store->MailboxOpen(MailboxName, readWrite))) {
	    selectedNamespace = store;
	}
    }
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
    unsigned result = 0;
    if (NULL != selectedNamespace) {
	result = selectedNamespace->MailboxMessageCount();
    }
    return result;
}

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

std::string Namespace::GetMailboxUserPath() const {
    std::string result = "";
    if (NULL != selectedNamespace) {
	result = selectedNamespace->GetMailboxUserPath();
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
    // SYZYGY -- I have to delete the maps
    // SYZYGY -- that includes all the registered mail stores
}

void Namespace::AddNamespace(NAMESPACE_TYPES type, const std::string &name, MailStore *handler, char separator) {
    if ("" == name) {
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

MailStore::MAIL_STORE_RESULT Namespace::DeleteMessage(const std::string &MailboxName, size_t uid) {
    MailStore::MAIL_STORE_RESULT result = GENERAL_FAILURE;
    MailStore *store = getNameSpace(MailboxName);
    if (NULL != store) {
	// SYZYGY -- I need to strip off the namespace, I think
	result = store->DeleteMessage(MailboxName, uid);
    }
    return result;
}
