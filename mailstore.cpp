#include "mailstore.hpp"

MailStore::MailStore(ImapSession *session) : m_session(session) {
    m_errnoFromLibrary = 0;
}

MailStore::~MailStore()
{}

std::string MailStore::TurnErrorCodeIntoString(MAIL_STORE_RESULT code) {
    const char *response[] = {
	"Success",
	"General Failure",
	"You cannot create the INBOX",
	"You cannot delete the INBOX",
	"The mailbox already exists",
	"The mailbox does not exist",
	"There is a problem with the path",
	"The mailbox has already been subscribed",
	"The mailbox is not subscribed",
	"The mailbox is in use",
	"The mailbox has subfolders",
	"The mailbox is not selectable",
	"The mailbox was opened read only",
	"No mailbox is open",
	"Message file open failed",
	"Message file write failed",
	"Message not found"
    };
    if (code < (sizeof(response) / sizeof(const char*))) {
	std::string result = response[code];
	if (0 != m_errnoFromLibrary) {
	    result += ": ";
	    result += strerror(m_errnoFromLibrary);
	}
	return result;
    }
    else {
	return "Invalid mailbox error code";
    }
}

NUMBER_LIST MailStore::MailboxMsnToUid(const NUMBER_LIST &msns) {
    NUMBER_LIST result;
    for (NUMBER_LIST::const_iterator i=msns.begin(); i!=msns.end(); ++i) {
	result.push_back(MailboxMsnToUid(*i));
    }
    return result;
}

unsigned long MailStore::MailboxMsnToUid(const unsigned long msn) {
    unsigned long result = 0;
    if (msn <= m_uidGivenMsn.size()) {
	result = m_uidGivenMsn[msn-1];
    }
    return result;
}

NUMBER_LIST MailStore::MailboxUidToMsn(const NUMBER_LIST &uids) {
    NUMBER_LIST result;
    for (NUMBER_LIST::const_iterator i=uids.begin(); i!=uids.end(); ++i) {
	result.push_back(MailboxUidToMsn(*i));
    }
}

unsigned long MailStore::MailboxUidToMsn(unsigned long uid) {
    unsigned long result = 0;

    MSN_TO_UID::const_iterator i = find(m_uidGivenMsn.begin(), m_uidGivenMsn.end(), uid);
    if (i != m_uidGivenMsn.end()) {
	result = (i - m_uidGivenMsn.begin()) + 1;
    }
    return result;
}


MailStore::MAIL_STORE_RESULT MailStore::MessageList(SEARCH_RESULT &msns) const {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
    msns.clear(); 
    if (NULL != m_openMailbox) {
	for (int i=0; i<m_uidGivenMsn.size(); ++i) {
	    msns.push_back(m_uidGivenMsn[i]);
	}
    }
    else {
	result = MAILBOX_NOT_OPEN;
    }
    return result;
}
