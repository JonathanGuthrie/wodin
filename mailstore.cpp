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

