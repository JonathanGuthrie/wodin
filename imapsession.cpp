///////////////////////////////////////////////////////////////////////////////
//
// imapsession.cpp : Implementation of ImapSession
//
// Simdesk, Inc. © 2002-2004
// Author: Jonathan Guthrie
//
///////////////////////////////////////////////////////////////////////////////


// I have a timer that can fire and handle several different cases:
//   Look for updated data for asynchronous notifies
//   ***DONE*** Delay for failed LOGIN's or AUTHENTICATIONS
//   ***DONE*** Handle idle timeouts, both for IDLE mode and inactivity in other modes.


// SYZYGY -- I should allow the setting of the keepalive socket option
// SYZYGY -- refactor the handling of options and do the /good/bad/continued stuff

#include <sstream>

#include <time.h>
#include <sys/fsuid.h>

#include "imapsession.hpp"
#include "imapserver.hpp"
#include "sasl.hpp"

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

// #include <iostream>
#include <list>

typedef enum
{
    SSV_ERROR,
    SSV_ALL,
    SSV_ANSWERED,
    SSV_BCC,
    SSV_BEFORE,
    SSV_BODY,
    SSV_CC,
    SSV_DELETED,
    SSV_FLAGGED,
    SSV_FROM,
    SSV_KEYWORD,
    SSV_NEW,
    SSV_OLD,
    SSV_ON,
    SSV_RECENT,
    SSV_SEEN,
    SSV_SINCE,
    SSV_SUBJECT,
    SSV_TEXT,
    SSV_TO,
    SSV_UNANSWERED,
    SSV_UNDELETED,
    SSV_UNFLAGGED,
    SSV_UNKEYWORD,
    SSV_UNSEEN,
    SSV_DRAFT,
    SSV_HEADER,
    SSV_LARGER,
    SSV_NOT,
    SSV_OR,
    SSV_SENTBEFORE,
    SSV_SENTON,
    SSV_SENTSINCE,
    SSV_SMALLER,
    SSV_UID,
    SSV_UNDRAFT,
    SSV_OPAREN,
    SSV_CPAREN
} SEARCH_SYMBOL_VALUES;
typedef std::map<insensitiveString, SEARCH_SYMBOL_VALUES> SEARCH_SYMBOL_T;

static SEARCH_SYMBOL_T searchSymbolTable;

typedef enum
{
    SAV_MESSAGES = 1,
    SAV_RECENT = 2,
    SAV_UIDNEXT = 4,
    SAV_UIDVALIDITY = 8,
    SAV_UNSEEN = 16
} STATUS_ATT_VALUES;
typedef std::map<insensitiveString, STATUS_ATT_VALUES> STATUS_SYMBOL_T;

static STATUS_SYMBOL_T statusSymbolTable;

typedef std::map<insensitiveString, MailStore::MAIL_STORE_FLAGS> FLAG_SYMBOL_T;

static FLAG_SYMBOL_T flagSymbolTable;

typedef enum
{
    UID_STORE,
    UID_FETCH,
    UID_SEARCH,
    UID_COPY
} UID_SUBCOMMAND_VALUES;
typedef std::map<std::string, UID_SUBCOMMAND_VALUES> UID_SUBCOMMAND_T;

static UID_SUBCOMMAND_T uidSymbolTable;

typedef enum
{
    FETCH_ALL,
    FETCH_FAST,
    FETCH_FULL,
    FETCH_BODY,
    FETCH_BODY_PEEK,
    FETCH_BODYSTRUCTURE,
    FETCH_ENVELOPE,
    FETCH_FLAGS,
    FETCH_INTERNALDATE,
    FETCH_RFC822,
    FETCH_RFC822_HEADER,
    FETCH_RFC822_SIZE,
    FETCH_RFC822_TEXT,
    FETCH_UID
} FETCH_NAME_VALUES;
typedef std::map<insensitiveString, FETCH_NAME_VALUES> FETCH_NAME_T;

static FETCH_NAME_T fetchSymbolTable;

typedef enum
{
    FETCH_BODY_BODY,
    FETCH_BODY_HEADER,
    FETCH_BODY_FIELDS,
    FETCH_BODY_NOT_FIELDS,
    FETCH_BODY_TEXT,
    FETCH_BODY_MIME
} FETCH_BODY_PARTS;

bool ImapSession::m_anonymousEnabled = false;

IMAPSYMBOLS ImapSession::m_symbols;

void ImapSession::BuildSymbolTables()
{
    MailMessage::BuildSymbolTable();

    symbol symbolToInsert;
    symbolToInsert.levels[0] = true;
    symbolToInsert.levels[1] = true;
    symbolToInsert.levels[2] = true;
    symbolToInsert.levels[3] = true;
    symbolToInsert.sendUpdatedStatus = true;
    symbolToInsert.flag = false;
    symbolToInsert.handler = &ImapSession::CapabilityHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("CAPABILITY", symbolToInsert));
    symbolToInsert.handler = &ImapSession::NoopHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("NOOP", symbolToInsert));
    symbolToInsert.handler = &ImapSession::LogoutHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("LOGOUT", symbolToInsert));

    symbolToInsert.levels[0] = true;
    symbolToInsert.levels[1] = false;
    symbolToInsert.levels[2] = false;
    symbolToInsert.levels[3] = false;
    symbolToInsert.sendUpdatedStatus = true;
    symbolToInsert.handler = &ImapSession::StarttlsHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("STARTTTLS", symbolToInsert));
    symbolToInsert.handler = &ImapSession::AuthenticateHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("AUTHENTICATE", symbolToInsert));
    symbolToInsert.handler = &ImapSession::LoginHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("LOGIN", symbolToInsert));

    symbolToInsert.levels[0] = false;
    symbolToInsert.levels[1] = true;
    symbolToInsert.levels[2] = true;
    symbolToInsert.levels[3] = false;
    symbolToInsert.sendUpdatedStatus = true;
    symbolToInsert.handler = &ImapSession::NamespaceHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("NAMESPACE", symbolToInsert));
    symbolToInsert.handler = &ImapSession::CreateHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("CREATE", symbolToInsert));
    symbolToInsert.handler = &ImapSession::DeleteHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("DELETE", symbolToInsert));
    symbolToInsert.handler = &ImapSession::RenameHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("RENAME", symbolToInsert));
    symbolToInsert.handler = &ImapSession::SubscribeHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("UNSUBSCRIBE", symbolToInsert));
    symbolToInsert.flag = true;
    symbolToInsert.handler = &ImapSession::SubscribeHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("SUBSCRIBE", symbolToInsert));
    symbolToInsert.handler = &ImapSession::ListHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("LIST", symbolToInsert));
    symbolToInsert.flag = false;
    m_symbols.insert(IMAPSYMBOLS::value_type("LSUB", symbolToInsert));
    symbolToInsert.handler = &ImapSession::StatusHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("STATUS", symbolToInsert));
    symbolToInsert.handler = &ImapSession::AppendHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("APPEND", symbolToInsert));
    // My justification for not allowing the updating of the status after the select and examine commands
    // is because they can act like a close if they're executed in the selected state, and it's not allowed
    // after a close as per RFC 3501 6.4.2
    symbolToInsert.sendUpdatedStatus = false;
    symbolToInsert.handler = &ImapSession::SelectHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("SELECT", symbolToInsert));
    symbolToInsert.flag = true;
    symbolToInsert.handler = &ImapSession::SelectHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("EXAMINE", symbolToInsert));

    symbolToInsert.levels[0] = false;
    symbolToInsert.levels[1] = false;
    symbolToInsert.levels[2] = true;
    symbolToInsert.levels[3] = false;
    symbolToInsert.sendUpdatedStatus = true;
    symbolToInsert.handler = &ImapSession::CheckHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("CHECK", symbolToInsert));
    symbolToInsert.handler = &ImapSession::ExpungeHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("EXPUNGE", symbolToInsert));
    symbolToInsert.handler = &ImapSession::UidHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("UID", symbolToInsert));
    symbolToInsert.sendUpdatedStatus = false;
    symbolToInsert.handler = &ImapSession::CopyHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("COPY", symbolToInsert));
    symbolToInsert.handler = &ImapSession::CloseHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("CLOSE", symbolToInsert));
    symbolToInsert.handler = &ImapSession::SearchHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("SEARCH", symbolToInsert));
    symbolToInsert.handler = &ImapSession::FetchHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("FETCH", symbolToInsert));
    symbolToInsert.handler = &ImapSession::StoreHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("STORE", symbolToInsert));

    // This is the symbol table for the search keywords
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("ALL",        SSV_ALL));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("ANSWERED",   SSV_ANSWERED));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("BCC",        SSV_BCC));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("BEFORE",     SSV_BEFORE));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("BODY",       SSV_BODY));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("CC",         SSV_CC));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("DELETED",    SSV_DELETED));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("FLAGGED",    SSV_FLAGGED));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("FROM",       SSV_FROM));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("KEYWORD",    SSV_KEYWORD));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("NEW",        SSV_NEW));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("OLD",        SSV_OLD));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("ON",         SSV_ON));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("RECENT",     SSV_RECENT));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SEEN",       SSV_SEEN));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SINCE",      SSV_SINCE));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SUBJECT",    SSV_SUBJECT));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("TEXT",       SSV_TEXT));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("TO",         SSV_TO));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNANSWERED", SSV_UNANSWERED));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNDELETED",  SSV_UNDELETED));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNFLAGGED",  SSV_UNFLAGGED));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNKEYWORD",  SSV_UNKEYWORD));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNSEEN",     SSV_UNSEEN));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("DRAFT",      SSV_DRAFT));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("HEADER",     SSV_HEADER));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("LARGER",     SSV_LARGER));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("NOT",        SSV_NOT));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("OR",         SSV_OR));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SENTBEFORE", SSV_SENTBEFORE));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SENTON",     SSV_SENTON));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SENTSINCE",  SSV_SENTSINCE));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SMALLER",    SSV_SMALLER));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UID",        SSV_UID));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNDRAFT",    SSV_UNDRAFT));

    statusSymbolTable.insert(STATUS_SYMBOL_T::value_type("MESSAGES",	SAV_MESSAGES));
    statusSymbolTable.insert(STATUS_SYMBOL_T::value_type("RECENT",	SAV_RECENT));
    statusSymbolTable.insert(STATUS_SYMBOL_T::value_type("UIDNEXT",	SAV_UIDNEXT));
    statusSymbolTable.insert(STATUS_SYMBOL_T::value_type("UIDVALIDITY",	SAV_UIDVALIDITY));
    statusSymbolTable.insert(STATUS_SYMBOL_T::value_type("UNSEEN",	SAV_UNSEEN));

    // These are only the standard flags.  User-defined flags must be handled differently
    flagSymbolTable.insert(FLAG_SYMBOL_T::value_type("ANSWERED", MailStore::IMAP_MESSAGE_ANSWERED));
    flagSymbolTable.insert(FLAG_SYMBOL_T::value_type("FLAGGED",  MailStore::IMAP_MESSAGE_FLAGGED));
    flagSymbolTable.insert(FLAG_SYMBOL_T::value_type("DELETED",  MailStore::IMAP_MESSAGE_DELETED));
    flagSymbolTable.insert(FLAG_SYMBOL_T::value_type("SEEN",     MailStore::IMAP_MESSAGE_SEEN));
    flagSymbolTable.insert(FLAG_SYMBOL_T::value_type("DRAFT",    MailStore::IMAP_MESSAGE_DRAFT));

    uidSymbolTable.insert(UID_SUBCOMMAND_T::value_type("STORE",  UID_STORE));
    uidSymbolTable.insert(UID_SUBCOMMAND_T::value_type("FETCH",  UID_FETCH));
    uidSymbolTable.insert(UID_SUBCOMMAND_T::value_type("SEARCH", UID_SEARCH));
    uidSymbolTable.insert(UID_SUBCOMMAND_T::value_type("COPY",   UID_COPY));

    fetchSymbolTable.insert(FETCH_NAME_T::value_type("ALL",           FETCH_ALL));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("FAST",          FETCH_FAST));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("FULL",          FETCH_FULL));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("BODY",          FETCH_BODY));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("BODY.PEEK",     FETCH_BODY_PEEK));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("BODYSTRUCTURE", FETCH_BODYSTRUCTURE));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("ENVELOPE",      FETCH_ENVELOPE));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("FLAGS",         FETCH_FLAGS));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("INTERNALDATE",  FETCH_INTERNALDATE));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("RFC822",        FETCH_RFC822));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("RFC822.HEADER", FETCH_RFC822_HEADER));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("RFC822.SIZE",   FETCH_RFC822_SIZE));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("RFC822.TEXT",   FETCH_RFC822_TEXT));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("UID",           FETCH_UID));
}

ImapSession::ImapSession(Socket *sock, ImapServer *server, SessionDriver *driver, unsigned failedLoginPause) : m_s(sock), m_server(server), m_driver(driver), m_failedLoginPause(failedLoginPause) {
    m_state = ImapNotAuthenticated;
    m_userData = NULL;
    m_LoginDisabled = false;
    m_lineBuffer = NULL;	// going with a static allocation for now
    m_lineBuffPtr = 0;
    m_lineBuffLen = 0;
    m_parseBuffer = NULL;
    m_parseBuffLen = 0;
    m_store = NULL;
    m_literalLength = 0;
    std::string response("* OK [");
    response += BuildCapabilityString() + "] IMAP4rev1 server ready\r\n";
    m_s->Send((uint8_t *) response.c_str(), response.length());
    m_lastCommandTime = time(NULL);
    m_purgedMessages.clear();
}

ImapSession::~ImapSession() {
    if (ImapSelected == m_state) {
	m_store->MailboxClose();
    }
    if (NULL != m_store) {
	delete m_store;
    }
    m_store = NULL;
    if (NULL != m_userData)
    {
	delete m_userData;
    }
    m_userData = NULL;
    std::string bye = "* BYE SimDesk IMAP4rev1 server shutting down\r\n";
    m_s->Send((uint8_t *)bye.data(), bye.size());
}


void ImapSession::atom(uint8_t *data, size_t dataLen, size_t &parsingAt) {
    static char include_list[] = "!#$&'+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[^_`abcdefghijklmnopqrstuvwxyz|}~";
 
    while (parsingAt < dataLen) {
	if (NULL != strchr(include_list, data[parsingAt])) {
	    uint8_t temp = toupper(data[parsingAt]);
	    AddToParseBuffer(&temp, 1, false);
	}
	else {
	    AddToParseBuffer(NULL, 0);
	    break;
	}
	++parsingAt;
    }
}

enum ImapStringState ImapSession::astring(uint8_t *data, size_t dataLen, size_t &parsingAt, bool makeUppercase,
					  const char *additionalChars) {
    ImapStringState result = ImapStringGood;
    if (parsingAt < dataLen) {
	if ('"' == data[parsingAt]) {
	    parsingAt++; // Skip the opening quote
	    bool notdone = true, escape_flag = false;
	    do {
		switch(data[parsingAt]) {
		case '"':
		    if (escape_flag) {
			AddToParseBuffer(&data[parsingAt], 1, false);
			escape_flag = false;
		    }
		    else {
			AddToParseBuffer(NULL, 0);
			notdone = false;
		    }
		    break;

		case '\\':
		    if (escape_flag) {
			AddToParseBuffer(&data[parsingAt], 1, false);
		    }
		    escape_flag = !escape_flag;
		    break;

		default:
		    escape_flag = false;
		    if (makeUppercase) {
			uint8_t temp = toupper(data[parsingAt]);
			AddToParseBuffer(&temp, 1, false);
		    }
		    else {
			AddToParseBuffer(&data[parsingAt], 1, false);
		    }
		}
		++parsingAt;
		// Check to see if I've run off the end of the input buffer
		if ((parsingAt > dataLen) || ((parsingAt == dataLen) && notdone)) {
		    notdone = false;
		    result = ImapStringBad;
		}
	    } while(notdone);
	}
	else {
	    if ('{' == data[parsingAt]) {
		// It's a literal string
		result = ImapStringPending;
		parsingAt++; // Skip the curly brace
		char *close;
		// read the number
		m_literalLength = strtoul((char *)&data[parsingAt], &close, 10);
		// check for the close curly brace and end of message and to see if I can fit all that data
		// into the buffer
		size_t lastChar = (size_t) (close - ((char *)data));
		if ((NULL == close) || ('}' != close[0]) || (lastChar != (dataLen - 1))) {
		    result = ImapStringBad;
		}
	    }
	    else {
		// It looks like an atom
		static char include_list[] = "!#$&'+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz|}~";
		result = ImapStringBad;

		while (parsingAt < dataLen) {
		    if ((NULL == strchr(include_list, data[parsingAt])) &&
			((NULL == additionalChars) || (NULL == strchr(additionalChars, data[parsingAt])))) {
			break;
		    }
		    result = ImapStringGood;
		    if (makeUppercase) {
			uint8_t temp = toupper(data[parsingAt]);
			AddToParseBuffer(&temp, 1, false);
		    }
		    else {
			AddToParseBuffer(&data[parsingAt], 1, false);
		    }
		    ++parsingAt;
		}
		AddToParseBuffer(NULL, 0);
	    }
	}
    }
    return result;
}


void ImapSession::AddToParseBuffer(const uint8_t *data, size_t length, bool nulTerminate) {
    // I need the +1 because I'm going to add a '\0' to the end of the string
    if ((m_parsePointer + length + 1) > m_parseBuffLen) {
	size_t newSize = MAX(m_parsePointer + length + 1, 2 * m_parseBuffLen);
	uint8_t *temp = new uint8_t[newSize];
	uint8_t *debug = m_parseBuffer;
	memcpy(temp, m_parseBuffer, m_parsePointer);
	delete[] m_parseBuffer;
	m_parseBuffLen = newSize;
	m_parseBuffer = temp;
    }
    if (0 < length) {
	memcpy(&m_parseBuffer[m_parsePointer], data, length);
    }
    m_parsePointer += length;
    m_parseBuffer[m_parsePointer] = '\0';
    if (nulTerminate) {
	m_parsePointer++;
    }
}

/*--------------------------------------------------------------------------------------*/
/* AsynchronousEvent									*/
/*--------------------------------------------------------------------------------------*/
void ImapSession::AsynchronousEvent(void) {
    if (ImapSelected == m_state) {
	NUMBER_SET purgedMessages;
	if (MailStore::SUCCESS == m_store->MailboxUpdateStats(&purgedMessages)) {
	    m_purgedMessages.insert(purgedMessages.begin(), purgedMessages.end());
	}
    }
}

/*--------------------------------------------------------------------------------------*/
/* ReceiveData										*/
/*--------------------------------------------------------------------------------------*/
int ImapSession::ReceiveData(uint8_t *data, size_t dataLen) {
    m_lastCommandTime = time(NULL);

    uint32_t newDataBase = 0;
    int result = 0;
    // Okay, I need to organize the data into lines.  Each line is however much data ended by a
    // CRLF.  In order to support this, I need a buffer to put data into.

    // First deal with the characters left over from the last call to ReceiveData, if any
    if (0 < m_lineBuffPtr) {
	bool crFlag = ('\r' == m_lineBuffer[m_lineBuffPtr]);

	for(; newDataBase<dataLen; ++newDataBase) {
	    // check lineBuffPtr against its current size
	    if (m_lineBuffPtr >= m_lineBuffLen) {
		// If it's currently not big enough, check its size against the biggest allowable
		if (m_lineBuffLen > IMAP_BUFFER_LEN) {
		    // The buffer is too big so I assume there is crap in the buffer, so I can ignore it.
		    // At this point, there is no demonstrably "right" thing to do, so anything I can do 
		    // is likely to be wrong, so doing something simple and wrong is slightly better than
		    // doing something complicated and wrong.
		    m_lineBuffPtr = 0;
		    delete[] m_lineBuffer;
		    m_lineBuffer = NULL;
		    m_lineBuffLen = 0;
		    break;
		}
		else {
		    // The buffer hasn't reached the absolute maximum size for processing, so I grow it
		    uint8_t *temp;
		    temp = new uint8_t[2 * m_lineBuffLen];
		    memcpy(temp, m_lineBuffer, m_lineBuffPtr);
		    delete[] m_lineBuffer;
		    m_lineBuffer = temp;
		    m_lineBuffLen *= 2;
		}

		// Eventually, I'll make the buffer bigger up to a point
	    }
	    m_lineBuffer[m_lineBuffPtr++] = data[newDataBase];
	    if (crFlag && ('\n' == data[newDataBase])) {
		result = HandleOneLine(m_lineBuffer, m_lineBuffPtr);
		m_lineBuffPtr = 0;
		delete[] m_lineBuffer;
		m_lineBuffer = NULL;
		m_lineBuffLen = 0;
		// This handles the increment to skip over the character that we would be doing,
		// in the for header, but we're breaking out of the loop so it has to be done
		// explicitly 
		++newDataBase;
		break;
	    }
	    crFlag = ('\r' == data[newDataBase]);
	}
    }

    // Next, deal with all the whole lines that have arrived in the current call to Receive Data
    // I also deal with literal strings here, because I don't have to break them into lines
    bool crFlag = false;
    bool notDone = true;
    uint32_t currentCommandStart = newDataBase;
    do {
	if (0 < m_literalLength) {
	    size_t bytesToFeed = m_literalLength;
	    if (bytesToFeed >= (dataLen - currentCommandStart)) {
		bytesToFeed = dataLen - currentCommandStart;
		notDone = false;
		result = HandleOneLine(&data[currentCommandStart], bytesToFeed);
		currentCommandStart += bytesToFeed;
		newDataBase = currentCommandStart;
	    }
	    else {
		// If I get here, then I know that I've got at least as many characters in
		// the input buffer as I need to fulfill the literal request.  However, I 
		// can't just pass that many characters to HandleOneLine because it may not
		// be the end of a line.  I deal with this by using the usual end-of-line logic
		// starting with the beginning of the string or two bytes before the end.
		if (bytesToFeed > 2) {
		    newDataBase += bytesToFeed - 2;
		}
		crFlag = false;
		for(; newDataBase<dataLen; ++newDataBase) {
		    if (crFlag && ('\n' == data[newDataBase])) {
			// The "+1"s here are due to the fact that I want to include the current character in the 
			// line to be handled
			result = HandleOneLine(&data[currentCommandStart], newDataBase - currentCommandStart + 1);
			currentCommandStart = newDataBase + 1;
			crFlag = false;
			break;
		    }
		    crFlag = ('\r' == data[newDataBase]);
		}
		notDone = (currentCommandStart < dataLen) && (newDataBase < dataLen);
	    }
	}
	else {
	    for(; newDataBase<dataLen; ++newDataBase) {
		if (crFlag && ('\n' == data[newDataBase])) {
		    // The "+1"s here are due to the fact that I want to include the current character in the 
		    // line to be handled
		    result = HandleOneLine(&data[currentCommandStart], newDataBase - currentCommandStart + 1);
		    currentCommandStart = newDataBase + 1;
		    crFlag = false;
		    break;
		}
		crFlag = ('\r' == data[newDataBase]);
	    }
	    notDone = (currentCommandStart < dataLen) && (newDataBase < dataLen);
	}
    } while (notDone);

    // Lastly, save any leftover data from the current call to ReceiveData
    // Eventually, I'll size the buffer for this result
    // for now, assume the message is garbage if it's too big
    if ((0 < (newDataBase - currentCommandStart)) && (IMAP_BUFFER_LEN > (newDataBase - currentCommandStart))) {
	m_lineBuffPtr = newDataBase - currentCommandStart;
	m_lineBuffLen = 2 * m_lineBuffPtr;
	m_lineBuffer = new uint8_t[m_lineBuffLen];
	memcpy(m_lineBuffer, &data[currentCommandStart], m_lineBuffPtr);
    }
    return result;
}

std::string ImapSession::FormatTaggedResponse(IMAP_RESULTS status, bool sendUpdatedStatus) {
    std::string response;

    if ((status != IMAP_NOTDONE) && sendUpdatedStatus && (ImapSelected == m_state)) {
	for (NUMBER_SET::iterator i=m_purgedMessages.begin() ; i!= m_purgedMessages.end(); ++i) {
	    int message_number;
	    std::ostringstream ss;

	    message_number = m_store->MailboxUidToMsn(*i);
	    ss << "* " << message_number << " EXPUNGE\r\n";
	    m_store->ExpungeThisUid(*i);
	    m_s->Send((uint8_t *)ss.str().c_str(), ss.str().size());
	}
	m_purgedMessages.clear();

	// I want to re-read the cached data if I'm in the selected state and
	// either the number of messages or the UIDNEXT value changes
	if ((m_currentNextUid != m_store->GetSerialNumber()) ||
	    (m_currentMessageCount != m_store->MailboxMessageCount())) {
	    std::ostringstream ss;

	    if (m_currentNextUid != m_store->GetSerialNumber()) {
		m_currentNextUid = m_store->GetSerialNumber();
		ss << "* OK [UIDNEXT " << m_currentNextUid << "]\r\n";
	    }
	    if (m_currentMessageCount != m_store->MailboxMessageCount()) {
		m_currentMessageCount = m_store->MailboxMessageCount();
		ss << "* " << m_currentMessageCount << " EXISTS\r\n";
	    }
	    if (m_currentRecentCount != m_store->MailboxRecentCount()) {
		m_currentRecentCount = m_store->MailboxRecentCount();
		ss << "* " << m_currentRecentCount << " RECENT\r\n";
	    }
	    if (m_currentUnseen != m_store->MailboxFirstUnseen()) {
		m_currentUnseen = m_store->MailboxFirstUnseen();
		if (0 < m_currentUnseen) {
		    ss << "* OK [UNSEEN " << m_currentUnseen << "]\r\n";
		}
	    }
	    if (m_currentUidValidity != m_store->GetUidValidityNumber()) {
		m_currentUidValidity = m_store->GetUidValidityNumber();
		ss << "* OK [UIDVALIDITY " << m_currentUidValidity << "]\r\n";
	    }
	    m_s->Send((uint8_t *)ss.str().c_str(), ss.str().size());
	}
    }

    switch(status) {
    case IMAP_OK:
	response = (char *)m_parseBuffer;
	response += " OK";
	response += m_responseCode[0] == '\0' ? "" : " ";
	response += (char *)m_responseCode;
	response += " ";
	response += (char *)&m_parseBuffer[m_commandString];
	response += " ";
	response += (char *)m_responseText;
	response += "\r\n";
	break;
 
    case IMAP_NO:
    case IMAP_NO_WITH_PAUSE:
	response = (char *)m_parseBuffer;
	response += " NO";
	response += m_responseCode[0] == '\0' ? "" : " ";
	response += (char *)m_responseCode;
	response += " ";
	response += (char *)&m_parseBuffer[m_commandString];
	response += " ";
	response += (char *)m_responseText;
	response += "\r\n";
	break;

    case IMAP_NOTDONE:
	response += "+";
	response += m_responseCode[0] == '\0' ? "" : " ";
	response += (char *)m_responseCode;
	response += m_responseText[0] == '\0' ? "" : " ";
	response += (char *)m_responseText;
	response += "\r\n";
	break;

    case IMAP_MBOX_ERROR:
	response = (char *)m_parseBuffer;
	response += " NO ";
	response += (char *)&m_parseBuffer[m_commandString];
	response += " \"";
	response += m_store->TurnErrorCodeIntoString(m_mboxErrorCode);
	response += "\"\r\n";
	break;

    case IMAP_IN_LITERAL:
	response = "";
	break;

    default:
    case IMAP_BAD:
	response = (char *)m_parseBuffer;
	response += " BAD";
	response += m_responseCode[0] == '\0' ? "" : " ";
	response += (char *)m_responseCode;
	response += " ";
	response += (char *)&m_parseBuffer[m_commandString];
	response += " ";
	response += (char *)m_responseText;
	response += "\r\n";
	break; 
    }

    return response;
}

// This function assumes that it is passed a single entire line each time.
// It returns true if it's expecting more data, and false otherwise
int ImapSession::HandleOneLine(uint8_t *data, size_t dataLen) {
    std::string response;
    int result = 0;
    size_t i;
    bool commandFound = false;

    if (NULL == m_currentHandler) {
	m_parseStage = 0;
	dataLen -= 2;  // It's not literal data, so I can strip the CRLF off the end
	data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	// std::cout << "In HandleOneLine, new command \"" << data << "\"" << std::endl;
	// There's nothing magic about 10, it just seems a convenient size
	// The initial value of parseBuffLen should be bigger than dwDataLen because
	// if it's the same size or smaller, it WILL have to be reallocated at some point.
	m_parseBuffLen = dataLen + 10;
	m_parseBuffer = new uint8_t[m_parseBuffLen];
	uint8_t *debug = m_parseBuffer;
	m_parsePointer = 0; // It's a new command, so I need to reset the pointer

	char *tagEnd = strchr((char *)data, ' ');
	if (NULL != tagEnd) {
	    i = (size_t) (tagEnd - ((char *)data));
	}
	else {
	    i = dataLen;
	}
	AddToParseBuffer(data, i);
	while ((i < dataLen) && (' ' == data[i])) {
	    ++i;  // Lose the space separating the tag from the command
	}
	if (i < dataLen) {
	    commandFound = true;
	    m_commandString = m_parsePointer;
	    tagEnd = strchr((char *)&data[i], ' ');
	    if (NULL != tagEnd) {
		AddToParseBuffer(&data[i], (size_t) (tagEnd - ((char *)&data[i])));
		i = (size_t) (tagEnd - ((char *)data));
	    }
	    else {
		AddToParseBuffer(&data[i], dataLen - i);
		i = dataLen;
	    }

	    m_arguments = m_parsePointer;
	    while ((i < dataLen) && (' ' == data[i])) {
		++i;  // Lose the space between the command and the arguments
	    }
	    // std::cout << "Looking up the command \"" << &m_parseBuffer[m_commandString] << "\"" << std::endl;
	    IMAPSYMBOLS::iterator found = m_symbols.find((char *)&m_parseBuffer[m_commandString]);
	    if ((found != m_symbols.end()) && found->second.levels[m_state]) {
		m_currentHandler = found->second.handler;
		m_sendUpdatedStatus = found->second.sendUpdatedStatus;
		m_flag = found->second.flag;
	    }
	}
    }
    if (NULL != m_currentHandler) {
	IMAP_RESULTS status;
	m_responseCode[0] = '\0';
	strncpy(m_responseText, "Completed", MAX_RESPONSE_STRING_LENGTH);
	status = (this->*m_currentHandler)(data, dataLen, i, m_flag);
	response = FormatTaggedResponse(status, m_sendUpdatedStatus);
	if ((IMAP_NOTDONE != status) && (IMAP_IN_LITERAL != status) && (IMAP_TRY_AGAIN != status)) {
	    m_currentHandler = NULL;
	}
	if (IMAP_NO_WITH_PAUSE == status) {
	    result = 1;
	    GetServer()->DelaySend(m_driver, m_failedLoginPause, response);
	}
	else {
	    if (IMAP_IN_LITERAL != status) {
		m_s->Send((uint8_t *)response.data(), response.length());
	    }
	}
    }
    else {
	if (commandFound) {
	    strncpy(m_responseText, "Unrecognized Command", MAX_RESPONSE_STRING_LENGTH);
	    m_responseCode[0] = '\0';
	    response = FormatTaggedResponse(IMAP_BAD, true);
	    m_s->Send((uint8_t *)response.data(), response.length());
	}
	else {
	    response = (char *)m_parseBuffer;
	    response += " BAD No Command\r\n";
	    m_s->Send((uint8_t *)response.data(), response.length());
	}
    }
    if (ImapLogoff <= m_state) {
	// If I'm logged off, I'm obviously not expecting any more data
	result = -1;
    }
    if (NULL == m_currentHandler) {
	m_literalLength = 0;
	if (NULL != m_parseBuffer) {
	    delete[] m_parseBuffer;
	    m_parseBuffer = NULL;
	    m_parseBuffLen = 0;
	}
    }

    return result;
}


/*--------------------------------------------------------------------------------------*/
/* ImapSession::UnimplementedHandler													*/
/*--------------------------------------------------------------------------------------*/
/* Gets called whenever the system doesn't know what else to do.  It signals the client */
/* that the command was not recognized													*/
/*--------------------------------------------------------------------------------------*/
IMAP_RESULTS ImapSession::UnimplementedHandler() {
    strncpy(m_responseText, "Unrecognized Command",  MAX_RESPONSE_STRING_LENGTH);
    return IMAP_BAD;
}


std::string ImapSession::BuildCapabilityString()
{
    std::string capability;
#if 0 // SYZYGY AUTH ANONYMOUS LOGINDISABLED all are determined by the server state as much as the session
    capability.Format("CAPABILITY IMAP4rev1 CHILDREN NAMESPACE AUTH=PLAIN%s%s",
		      m_AnonymousEnabled ? " AUTH=ANONYMOUS" : "",
		      m_LoginDisabled ? " LOGINDISABLED" : "");
#else // !0
    capability = "CAPABILITY IMAP4rev1 CHILDREN NAMESPACE AUTH=PLAIN";
#endif // !0

    return capability;
}


/*
 * The capability handler looks simple, but there's some complexity lurking
 * there.  For starters, I'm going to return a constant string with not very
 * much in it.  However, later on the string might change depending on the
 * context.  Further, since the capability string might be sent by multiple
 * different commands, that actual work will be done by a worker function.
 */
IMAP_RESULTS ImapSession::CapabilityHandler(uint8_t *pData, size_t dataLen, size_t &r_dwParsingAt, bool unused)
{
    std::string response("* ");
    response += BuildCapabilityString() + "\r\n";
    m_s->Send((uint8_t *)response.c_str(), response.size());
    return IMAP_OK;
}

/*
 * The NOOP may be used by the server to trigger things like checking for
 * new messages although the server doesn't have to wait for this to do
 * things like that
 */
IMAP_RESULTS ImapSession::NoopHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    // This command literally doesn't do anything.  If there was an update, it was found
    // asynchronously and the updated info that will be printed in FormatTaggedResponse

    return IMAP_OK;
}


/*
 * The LOGOUT indicates that the session may be closed.  The main command
 * processor gets the word about this because the state is set to "4" which
 * means "logout state"
 */
IMAP_RESULTS ImapSession::LogoutHandler(uint8_t *pData, size_t dataLen, size_t &r_dwParsingAt, bool unused) {
    // If the mailbox is open, close it
    // In IMAP, deleted messages are always purged before a close
    if (ImapSelected == m_state) {
	m_store->MailboxClose();
    }
    if (ImapNotAuthenticated < m_state) {
	if (NULL != m_store) {
	    delete m_store;
	    m_store = NULL;
	}
    }
    m_state = ImapLogoff;
    std::string bye = "* BYE IMAP4rev1 server closing\r\n";
    m_s->Send((uint8_t *)bye.data(), bye.size());
    return IMAP_OK;
}

IMAP_RESULTS ImapSession::StarttlsHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    return UnimplementedHandler();
}

IMAP_RESULTS ImapSession::AuthenticateHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    IMAP_RESULTS result; 
    switch (m_parseStage) {
    case 0:
	m_userData = m_server->GetUserInfo((char *)&m_parseBuffer[m_arguments]);
	atom(data, dataLen, parsingAt);
	m_auth = SaslChooser(this, (char *)&m_parseBuffer[m_arguments]);
	if (NULL != m_auth) {
	    m_auth->SendChallenge(m_responseCode);
	    m_responseText[0] = '\0';
	    m_parseStage = 1;
	    result = IMAP_NOTDONE;
	}
	else {
	    strncpy(m_responseText, "Unrecognized Authentication Method", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_NO;
	}
	break;

    case 1:
	std::string str((char*)data, dataLen - 2);

	switch (m_auth->ReceiveResponse(str)) {
	case Sasl::ok:
	    result = IMAP_OK;
	    // user->GetUserFileSystem();
	    m_userData = m_server->GetUserInfo(m_auth->getUser().c_str());

	    if (NULL == m_store) {
		m_store = m_server->GetMailStore(this);
	    }
	    m_store->CreateMailbox("INBOX");

	    // Log("Client %u logged-in user %s %lu\n", m_dwClientNumber, m_pUser->m_szUsername.c_str(), m_pUser->m_nUserID);

	    m_state = ImapAuthenticated;
	    break;

	case Sasl::no:
	    result = IMAP_NO_WITH_PAUSE;
	    m_state = ImapNotAuthenticated;
	    strncpy(m_responseText, "Authentication Failed", MAX_RESPONSE_STRING_LENGTH);
	    break;

	case Sasl::bad:
	default:
	    result = IMAP_BAD;
	    m_state = ImapNotAuthenticated;
	    strncpy(m_responseText, "Authentication Cancelled", MAX_RESPONSE_STRING_LENGTH);
	    break;
	}
	delete m_auth;
	m_auth = NULL;
	break;
    }
    return result;
}


IMAP_RESULTS ImapSession::LoginHandlerExecute() {
    IMAP_RESULTS result;

    m_userData = m_server->GetUserInfo((char *)&m_parseBuffer[m_arguments]);
    bool v = m_userData->CheckCredentials((char *)&m_parseBuffer[m_arguments+(strlen((char *)&m_parseBuffer[m_arguments])+1)]);
    if (v)
    {
	m_responseText[0] = '\0';
	// SYZYGY LOG
	// Log("Client %u logged-in user %s %lu\n", m_dwClientNumber, m_pUser->m_szUsername.c_str(), m_pUser->m_nUserID);
	if (NULL == m_store) {
	    m_store = m_server->GetMailStore(this);
	}
	m_store->CreateMailbox("INBOX");
	// store->FixMailstore(); // I only un-comment this, if I have reason to believe that mailboxes are broken

	// SYZYGY LOG
	// Log("User %lu logged in\n", m_pUser->m_nUserID);
	m_state = ImapAuthenticated;
	result = IMAP_OK;
    }
    else {
	delete m_userData;
	m_userData = NULL;
	strncpy(m_responseText, "Authentication Failed", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_NO_WITH_PAUSE;
    }
    return result;
}


/*
 * Okay, the login command's use is officially deprecated.  Instead, you're supposed
 * to use the AUTHENTICATE command with some SASL mechanism.  Since I'm trying to
 * duplicate the functionality of SimDesk's current IMAP server, I will include it,
 * but I'll also include a check of the login_disabled flag, which will set whether or
 * not this command is accepted by the command processor
 */
IMAP_RESULTS ImapSession::LoginHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    IMAP_RESULTS result;

    if (m_LoginDisabled) {
	strncpy(m_responseText, "Login Disabled", MAX_RESPONSE_STRING_LENGTH);
	return IMAP_NO;
    }
    else {
	result = IMAP_OK;
	if (0 < m_literalLength) {
	    if (dataLen >= m_literalLength) {
		AddToParseBuffer(data, m_literalLength);
		++m_parseStage;
		size_t i = m_literalLength;
		m_literalLength = 0;
		if ((i < dataLen) && (' ' == data[i])) {
		    ++i;
		}
		if (2 < dataLen) {
		    // Get rid of the CRLF if I have it
		    dataLen -= 2;
		    data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
		}
	    }
	    else {
		result = IMAP_IN_LITERAL;
		AddToParseBuffer(data, dataLen, false);
		m_literalLength -= dataLen;
	    }
	}
	if ((0 == m_literalLength) && (parsingAt < dataLen)) {
	    do {
		switch (astring(data, dataLen, parsingAt, false, NULL)) {
		case ImapStringGood:
		    ++m_parseStage;
		    if ((parsingAt < dataLen) && (' ' == data[parsingAt])) {
			++parsingAt;
		    }
		    break;

		case ImapStringBad:
		    result = IMAP_BAD;
		    break;

		case ImapStringPending:
		    result = IMAP_NOTDONE;
		    break;
		}
	    } while((IMAP_OK == result) && (parsingAt < dataLen));
	}
    }

    switch(result) {
    case IMAP_OK:
	if (2 == m_parseStage) {
	    result = LoginHandlerExecute();
	}
	else {
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
	break;

    case IMAP_NOTDONE:
	strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	break;

    case IMAP_BAD:
	strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	break;

    case IMAP_IN_LITERAL:
	break;

    default:
	strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}


IMAP_RESULTS ImapSession::NamespaceHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    // SYZYGY parsingAt should be the index of a '\0'
    std::string str = "* NAMESPACE " + m_store->ListNamespaces();
    str += "\n";
    m_s->Send((uint8_t *) str.c_str(), str.size());
    return IMAP_OK;
}


void ImapSession::SendSelectData(const std::string &mailbox, bool isReadWrite) {
    std::ostringstream ss;
    m_currentNextUid = m_store->GetSerialNumber();
    m_currentUidValidity = m_store->GetUidValidityNumber();
    m_currentMessageCount = m_store->MailboxMessageCount();
    m_currentRecentCount = m_store->MailboxRecentCount();
    m_currentUnseen = m_store->MailboxFirstUnseen();

    // The select commands sends
    // an untagged flags response
    // SYZYGY -- I need to generate the flags list
    ss << "* FLAGS (\\Draft \\Answered \\Flagged \\Deleted \\Seen)\r\n";
    // an untagged exists response
    // the exists response needs the number of messages in the message base
    ss << "* " << m_currentMessageCount << " EXISTS\r\n";
    // an untagged recent response
    // the recent response needs the count of recent messages
    ss << "* " << m_currentRecentCount << " RECENT\r\n";
    // an untagged okay unseen response
    if (0 < m_currentUnseen) {
	ss << "* OK [UNSEEN " << m_currentUnseen << "]\r\n";
    }
    // an untagged okay permanent flags response
    if (isReadWrite) {
	// SYZYGY -- I need to generate the permanentflags list
	ss << "* OK [PERMANENTFLAGS (\\Draft \\Answered \\Flagged \\Deleted \\Seen)]\r\n";
    }
    else {
	ss << "* OK [PERMANENTFLAGS ()]\r\n";
    }
    // an untagged okay uidnext response
    ss << "* OK [UIDNEXT " << m_currentNextUid << "]\r\n";
    // an untagged okay uidvalidity response
    ss << "* OK [UIDVALIDITY " << m_currentUidValidity << "]\r\n"; 
    m_s->Send((uint8_t *)ss.str().c_str(), ss.str().size());
}


IMAP_RESULTS ImapSession::SelectHandlerExecute(bool openReadWrite) {
    IMAP_RESULTS result = IMAP_MBOX_ERROR;

    if (ImapSelected == m_state) {
	// If the mailbox is open, close it
	if (MailStore::CANNOT_COMPLETE_ACTION == m_store->MailboxClose()) {
	    result = IMAP_TRY_AGAIN;
	}
	else {
	    m_state = ImapAuthenticated;
	}
    }
    if (ImapAuthenticated == m_state) {
	std::string mailbox = (char *)&m_parseBuffer[m_arguments];
	switch (m_mboxErrorCode = m_store->MailboxOpen(mailbox, openReadWrite)) {
	case MailStore::SUCCESS:
	    SendSelectData(mailbox, openReadWrite);
	    if (openReadWrite) {
		strncpy(m_responseCode, "[READ-WRITE]", MAX_RESPONSE_STRING_LENGTH);
	    }
	    else {
		strncpy(m_responseCode, "[READ-ONLY]", MAX_RESPONSE_STRING_LENGTH);
	    }
	    result = IMAP_OK;
	    m_state = ImapSelected;
	    break;

	case MailStore::CANNOT_COMPLETE_ACTION:
	    result = IMAP_TRY_AGAIN;
	    break;
	}
    }

    return result;
}


/*
 * In this function, parse stage 0 is before the name
 * parse stage 1 is during the name
 * parse stage 2 is after the second name
 */
IMAP_RESULTS ImapSession::SelectHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool isReadOnly) {
    IMAP_RESULTS result = IMAP_OK;

    switch (m_parseStage) {
    case 0:
	switch (astring(data, dataLen, parsingAt, false, NULL)) {
	case ImapStringGood:
	    m_parseStage = 2;
	    result = SelectHandlerExecute(isReadOnly);
	    break;

	case ImapStringBad:
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	    break;

	case ImapStringPending:
	    result = IMAP_NOTDONE;
	    ++m_parseStage;
	    strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	    break;

	default:
	    strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	    break;
	}
	break;

    case 1:
	if (dataLen >= m_literalLength) {
	    m_parseStage = 2;
	    AddToParseBuffer(data, m_literalLength);
	    size_t i = m_literalLength;
	    m_literalLength = 0;
	    if ((i < dataLen) && (' ' == data[i])) {
		++i;
	    }
	    if (2 < dataLen) {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    result = SelectHandlerExecute(isReadOnly);
	}
	else {
	    result = IMAP_IN_LITERAL;
	    AddToParseBuffer(data, dataLen, false);
	    m_literalLength -= dataLen;
	}
	break;

    default:
	result = SelectHandlerExecute(isReadOnly);
	break;
    }

    return result;
}


IMAP_RESULTS ImapSession::CreateHandlerExecute() {
    IMAP_RESULTS result = IMAP_MBOX_ERROR;
    // SYZYGY -- check to make sure that the argument list has just the one argument
    // SYZYGY -- parsingAt should point to '\0'
    std::string mailbox((char *)&m_parseBuffer[m_arguments]);
    switch (m_mboxErrorCode = m_store->CreateMailbox(mailbox)) {
    case MailStore::SUCCESS:
	result = IMAP_OK;
	break;

    case MailStore::CANNOT_COMPLETE_ACTION:
	result = IMAP_TRY_AGAIN;
	break;
    }
    return result;
}


/*
 * In this function, parse stage 0 is before the name
 * parse stage 1 is during the name
 * parse stage 2 is after the second name
 */
IMAP_RESULTS ImapSession::CreateHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    IMAP_RESULTS result = IMAP_OK;

    switch (m_parseStage) {
    case 0:
	switch (astring(data, dataLen, parsingAt, false, NULL)) {
	case ImapStringGood:
	    m_parseStage = 2;
	    result = CreateHandlerExecute();
	    break;

	case ImapStringBad:
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	    break;

	case ImapStringPending:
	    result = IMAP_NOTDONE;
	    ++m_parseStage;
	    strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	    break;

	default:
	    strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	    break;
	}
	break;

    case 1:
	if (dataLen >= m_literalLength) {
	    m_parseStage = 2;
	    AddToParseBuffer(data, m_literalLength);
	    size_t i = m_literalLength;
	    m_literalLength = 0;
	    if ((i < dataLen) && (' ' == data[i])) {
		++i;
	    }
	    if (2 < dataLen) {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    result = CreateHandlerExecute();
	}
	else {
	    result = IMAP_IN_LITERAL;
	    AddToParseBuffer(data, dataLen, false);
	    m_literalLength -= dataLen;
	}
	break;

    default:
	result = CreateHandlerExecute();
	break;
    }
    return result;
}

IMAP_RESULTS ImapSession::DeleteHandlerExecute() {
    IMAP_RESULTS result = IMAP_MBOX_ERROR;
    // SYZYGY -- check to make sure that the argument list has just the one argument
    // SYZYGY -- parsingAt should point to '\0'
    std::string mailbox((char *)&m_parseBuffer[m_arguments]);
    switch (m_mboxErrorCode = m_store->DeleteMailbox(mailbox)) {
    case MailStore::SUCCESS:
	result = IMAP_OK;
	break;

    case MailStore::CANNOT_COMPLETE_ACTION:
	result = IMAP_TRY_AGAIN;
	break;
    }
    return result;
}

/*
 * In this function, parse stage 0 is before the name
 * parse stage 1 is during the name
 * parse stage 2 is after the second name
 */
IMAP_RESULTS ImapSession::DeleteHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    IMAP_RESULTS result = IMAP_OK;

    switch (m_parseStage) {
    case 0:
	// SYZYGY -- check to make sure that the argument list has just the one argument
	switch (astring(data, dataLen, parsingAt, false, NULL)) {
	case ImapStringGood:
	    m_parseStage = 2;
	    result = DeleteHandlerExecute();
	    break;

	case ImapStringBad:
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	    break;

	case ImapStringPending:
	    result = IMAP_NOTDONE;
	    ++m_parseStage;
	    strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	    break;

	default:
	    strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	    break;
	}
	break;

    case 1:
	if (dataLen >= m_literalLength) {
	    m_parseStage = 2;
	    AddToParseBuffer(data, m_literalLength);
	    size_t i = m_literalLength;
	    m_literalLength = 0;
	    if ((i < dataLen) && (' ' == data[i])) {
		++i;
	    }
	    if (2 < dataLen) {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    result = DeleteHandlerExecute();
	}
	else {
	    result = IMAP_IN_LITERAL;
	    AddToParseBuffer(data, dataLen, false);
	    m_literalLength -= dataLen;
	}
	break;

    default:
	result = DeleteHandlerExecute();
	break;
    }
    return result;
}


/*
 * In this function, parse stage 0 is before the first name
 * parse stage 1 is during the first name
 * parse stage 2 is after the first name, and before the second
 * parse stage 3 is during the second name
 * parse stage 4 is after the second name
 */
IMAP_RESULTS ImapSession::RenameHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    IMAP_RESULTS result = IMAP_OK;

    do {
	switch (m_parseStage) {
	case 0:
	case 2:
	    switch (astring(data, dataLen, parsingAt, false, NULL)) {
	    case ImapStringGood:
		m_parseStage += 2;
		if ((parsingAt < dataLen) && (' ' == data[parsingAt])) {
		    ++parsingAt;
		}
		break;

	    case ImapStringBad:
		result = IMAP_BAD;
		break;

	    case ImapStringPending:
		++m_parseStage;
		result = IMAP_NOTDONE;
		break;
	    }
	    break;

	case 1:
	case 3:
	    if (dataLen >= m_literalLength) {
		++m_parseStage;
		AddToParseBuffer(data, m_literalLength);
		size_t i = m_literalLength;
		m_literalLength = 0;
		if ((i < dataLen) && (' ' == data[i])) {
		    ++i;
		}
		if (2 < dataLen) {
		    // Get rid of the CRLF if I have it
		    dataLen -= 2;
		    data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
		}
	    }
	    else {
		result = IMAP_IN_LITERAL;
		AddToParseBuffer(data, dataLen, false);
		m_literalLength -= dataLen;
	    }
	    break;

	default: {
	    std::string source((char *)&m_parseBuffer[m_arguments]);
	    std::string destination((char *)&m_parseBuffer[m_arguments+(strlen((char *)&m_parseBuffer[m_arguments])+1)]);

	    switch (m_mboxErrorCode = m_store->RenameMailbox(source, destination)) {
	    case MailStore::SUCCESS:
		result = IMAP_OK;
		break;

	    case MailStore::CANNOT_COMPLETE_ACTION:
		result = IMAP_TRY_AGAIN;
		break;
	    }
	}
	    break;
	}
    } while ((IMAP_OK == result) && (m_parseStage < 4));

    return result;
}

IMAP_RESULTS ImapSession::SubscribeHandlerExecute(bool isSubscribe) {
    IMAP_RESULTS result = IMAP_MBOX_ERROR;
    // SYZYGY -- check to make sure that the argument list has just the one argument
    // SYZYGY -- parsingAt should point to '\0'
    std::string mailbox((char *)&m_parseBuffer[m_arguments]);

    switch (m_mboxErrorCode = m_store->SubscribeMailbox(mailbox, isSubscribe)) {
    case MailStore::SUCCESS:
	result = IMAP_OK;
	break;

    case MailStore::CANNOT_COMPLETE_ACTION:
	result = IMAP_TRY_AGAIN;
	break;
    }
    return result;
}

IMAP_RESULTS ImapSession::SubscribeHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool isSubscribe) {
    IMAP_RESULTS result = IMAP_OK;

    switch (m_parseStage) {
    case 0:
	switch (astring(data, dataLen, parsingAt, false, NULL)) {
	case ImapStringGood:
	    result = SubscribeHandlerExecute(isSubscribe);
	    break;

	case ImapStringBad:
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	    break;

	case ImapStringPending:
	    result = IMAP_NOTDONE;
	    ++m_parseStage;
	    strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	    break;

	default:
	    strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	    break;
	}
	break;

    case 1:
	if (dataLen >= m_literalLength) {
	    m_parseStage = 2;
	    AddToParseBuffer(data, m_literalLength);
	    size_t i = m_literalLength;
	    m_literalLength = 0;
	    if ((i < dataLen) && (' ' == data[i])) {
		++i;
	    }
	    if (2 < dataLen) {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    result = SubscribeHandlerExecute(isSubscribe);
	}
	else {
	    result = IMAP_IN_LITERAL;
	    AddToParseBuffer(data, dataLen, false);
	    m_literalLength -= dataLen;
	}
	break;

    default:
	result = SubscribeHandlerExecute(isSubscribe);
    }
    return result;
}


static std::string ImapQuoteString(const std::string &to_be_quoted) {
    std::string result("\"");
    for (int i=0; i<to_be_quoted.size(); ++i) {
	if ('\\' == to_be_quoted[i]) {
	    result += '\\';
	}
	result += to_be_quoted[i];
    }
    result += '"';
    return result;
}


// Generate the mailbox flags for the list command here
static std::string GenMailboxFlags(const uint32_t attr) {
    std::string result;

    if (0 != (attr & MailStore::IMAP_MBOX_NOSELECT)) {
	result = "(\\Noselect ";
    }
    else {
	if (0 != (attr & MailStore::IMAP_MBOX_MARKED)) {
	    result += "(\\Marked ";
	}
	else {
	    if (0 != (attr & MailStore::IMAP_MBOX_UNMARKED)) {
		result += "(\\UnMarked ";
	    }
	    else {
		result += "(";
	    }
	}
    }
    if (0 != (attr & MailStore::IMAP_MBOX_NOINFERIORS)) {
	result += "\\NoInferiors)";
    }
    else {
	if (0 != (attr & MailStore::IMAP_MBOX_HASCHILDREN)) {
	    result += "\\HasChildren)";
	}
	else {
	    result += "\\HasNoChildren)";
	}
    }
    return result;
}

IMAP_RESULTS ImapSession::ListHandlerExecute(bool listAll) {
    IMAP_RESULTS result = IMAP_OK;

    std::string reference = (char *)&m_parseBuffer[m_arguments];
    std::string mailbox = (char *)&m_parseBuffer[m_arguments+(strlen((char *)&m_parseBuffer[m_arguments])+1)];

    if ((0 == reference.size()) && (0 == mailbox.size())) {
	if (listAll) {
	    // It's a request to return the base and the hierarchy delimiter
	    std::string response("* LIST () \"/\" \"\"\r\n");
	    m_s->Send((uint8_t *)response.c_str(), response.size());
	}
    }
    else {
	/*
	 * What I think I want to do here is combine the strings by checking the last character of the 
	 * reference for '.', adding it if it's not there, and then just combining the two strings.
	 *
	 * No, I can't do that.  I don't know what the separator is until I know what namespace it's in.
	 * the best I can do is merge them and hope for the best.
	 */
	reference += mailbox;
	MAILBOX_LIST mailboxList;
	m_store->BuildMailboxList(reference, &mailboxList, listAll);
	std::string response;
	for (MAILBOX_LIST::const_iterator i = mailboxList.begin(); i != mailboxList.end(); ++i) {
	    if (listAll) {
		response = "* LIST ";
	    }
	    else {
		response = "* LSUB ";
	    }
	    MAILBOX_NAME which = *i;
	    response += GenMailboxFlags(i->attributes) + " \"/\" " + ImapQuoteString(i->name) + "\r\n";
	    m_s->Send((uint8_t *)response.c_str(), response.size());
	}
    }
    return IMAP_OK;
}

IMAP_RESULTS ImapSession::ListHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool listAll) {
    IMAP_RESULTS result = IMAP_OK;
    if (0 == m_parseStage) {
	do {
	    enum ImapStringState state;
	    if (0 == m_parseStage) {
		state = astring(data, dataLen, parsingAt, false, NULL);
	    }
	    else {
		state = astring(data, dataLen, parsingAt, false, "%*");
	    }
	    switch (state) {
	    case ImapStringGood:
		++m_parseStage;
		if ((parsingAt < dataLen) && (' ' == data[parsingAt])) {
		    ++parsingAt;
		}
		break;

	    case ImapStringBad:
		result = IMAP_BAD;
		break;

	    case ImapStringPending:
		result = IMAP_NOTDONE;
		break;
	    }
	} while((IMAP_OK == result) && (parsingAt < dataLen));

	switch(result) {
	case IMAP_OK:
	    if (2 == m_parseStage) {
		result = ListHandlerExecute(listAll);
	    }
	    else {
		strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		result = IMAP_BAD;
	    }
	    break;

	case IMAP_NOTDONE:
	    strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	    break;

	case IMAP_BAD:
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    break;

	default:
	    strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	    break;
	}
    }
    else {
	if (dataLen >= m_literalLength) {
	    ++m_parseStage;
	    AddToParseBuffer(data, m_literalLength);
	    size_t i = m_literalLength;
	    m_literalLength = 0;
	    if ((i < dataLen) && (' ' == data[i])) {
		++i;
	    }
	    if (2 < dataLen) {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    while((2 > m_parseStage) && (IMAP_OK == result) && (i < dataLen)) {
		enum ImapStringState state;
		if (0 == m_parseStage) {
		    state = astring(data, dataLen, i, false, NULL);
		}
		else {
		    state = astring(data, dataLen, i, false, "%*");
		}
		switch (state) {
		case ImapStringGood:
		    ++m_parseStage;
		    if ((i < dataLen) && (' ' == data[i])) {
			++i;
		    }
		    break;

		case ImapStringBad:
		    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;

		case ImapStringPending:
		    strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_NOTDONE;
		    break;
		}
	    }
	    if (IMAP_OK == result) {
		if (2 == m_parseStage) {
		    result = ListHandlerExecute(listAll);
		}
		else {
		    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		}
	    }
	}
	else {
	    result = IMAP_IN_LITERAL;
	    AddToParseBuffer(data, dataLen, false);
	    m_literalLength -= dataLen;
	}
    }
    return result;
}


IMAP_RESULTS ImapSession::StatusHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    IMAP_RESULTS result = IMAP_OK;
    m_mailFlags = 0;

    switch (m_parseStage) {
    case 0:
	switch (astring(data, dataLen, parsingAt, false, NULL)) {
	case ImapStringGood:
	    m_parseStage = 2;
	    break;

	case ImapStringBad:
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	    break;

	case ImapStringPending:
	    result = IMAP_NOTDONE;
	    strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	    break;

	default:
	    strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	    break;
	}
	break;

    case 1:
	if (dataLen >= m_literalLength) {
	    m_parseStage = 2;
	    AddToParseBuffer(data, m_literalLength);
	    size_t i = m_literalLength;
	    m_literalLength = 0;
	    if (2 < dataLen) {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    m_responseText[0] = '\0';
	}
	else {
	    result = IMAP_IN_LITERAL;
	    AddToParseBuffer(data, dataLen, false);
	    m_literalLength -= dataLen;
	}
	break;

    default:
	break;
    }
    if (2 == m_parseStage) {
	// At this point, I've seen the mailbox name.  The next stuff are all in the pData buffer.
	// I expect to see a space then a paren
	if ((2 < (dataLen - parsingAt)) && (' ' == data[parsingAt]) && ('(' == data[parsingAt+1])) {
	    parsingAt += 2;
	    while (parsingAt < (dataLen-1)) {
		size_t len, next;
		char *p = strchr((char *)&data[parsingAt], ' ');
		if (NULL != p) {
		    len = (p - ((char *)&data[parsingAt]));
		    next = parsingAt + len + 1; // This one needs to skip the space
		}
		else {
		    len = dataLen - parsingAt - 1;
		    next = parsingAt + len;     // this one needs to keep the parenthesis
		}
		std::string s((char *)&data[parsingAt], len);
		STATUS_SYMBOL_T::iterator i = statusSymbolTable.find(s.c_str());
		if (i != statusSymbolTable.end()) {
		    m_mailFlags |= i->second;
		}
		else {
		    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    // I don't need to see any more, it's all bogus
		    break;
		}
		parsingAt = next;
	    }
	}
	if ((result == IMAP_OK) && (')' == data[parsingAt]) && ('\0' == data[parsingAt+1])) {
	    m_parseStage = 3;
	}
	else {
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
    }
    
    if (3 == m_parseStage) {
	std::string mailbox((char *)&m_parseBuffer[m_arguments]);
	std::string mailboxExternal = mailbox;
	unsigned messageCount, recentCount, uidNext, uidValidity, firstUnseen;

	switch (m_mboxErrorCode = m_store->GetMailboxCounts(mailbox, m_mailFlags, messageCount, recentCount, uidNext, uidValidity, firstUnseen)) {
	case MailStore::SUCCESS:
	{
	    std::ostringstream response;
	    response << "* STATUS ";
	    response << mailboxExternal << " ";
	    char separator = '(';
	    if (m_mailFlags & SAV_MESSAGES) {
		response << separator << "MESSAGES " << messageCount;
		separator = ' ';
	    } 
	    if (m_mailFlags & SAV_RECENT) {
		response << separator << "RECENT " << recentCount;
		separator = ' ';
	    }
	    if (m_mailFlags & SAV_UIDNEXT) {
		response << separator << "UIDNEXT " << uidNext;
		separator = ' ';
	    }
	    if (m_mailFlags & SAV_UIDVALIDITY) {
		response << separator << "UIDVALIDITY " << uidValidity;
		separator = ' ';
	    }
	    if (m_mailFlags & SAV_UNSEEN) {
		response << separator << "UNSEEN " << firstUnseen;
		separator = ' ';
	    }
	    response << ")\r\n";
	    m_s->Send((uint8_t *)response.str().data(), response.str().length());
	    result = IMAP_OK;
	}
	break;

	case MailStore::CANNOT_COMPLETE_ACTION:
	    result = IMAP_TRY_AGAIN;
	    break;

	default:
	    result = IMAP_MBOX_ERROR;
	    break;
	}
    }
    return result;
}

size_t ImapSession::ReadEmailFlags(uint8_t *data, size_t dataLen, size_t &parsingAt, bool &okay) {
    okay = true;
    uint32_t result = 0;

    // Read atoms until I run out of space or the next character is a ')' instead of a ' '
    while ((parsingAt <= dataLen) && (')' != data[parsingAt])) {
	char *str = (char *)&m_parseBuffer[m_parsePointer];
	if ('\\' == data[parsingAt++]) {
	    atom(data, dataLen, parsingAt);
	    FLAG_SYMBOL_T::iterator i = flagSymbolTable.find(str);
	    if (flagSymbolTable.end() != i) {
		result |= i->second;
	    }
	    else {
		okay = false;
		break;
	    }
	}
	else {
	    okay = false;
	    break;
	}
	if ((parsingAt < dataLen) && (' ' == data[parsingAt])) {
	    ++parsingAt;
	}
    }
    return result;
}

IMAP_RESULTS ImapSession::AppendHandlerExecute(uint8_t *data, size_t dataLen, size_t &parsingAt) {
    IMAP_RESULTS result = IMAP_BAD;
    DateTime messageDateTime;

    if ((2 < (dataLen - parsingAt)) && (' ' == data[parsingAt++])) {
	if ('(' == data[parsingAt]) {
	    ++parsingAt;
	    bool flagsOk;
	    m_mailFlags = ReadEmailFlags(data, dataLen, parsingAt, flagsOk);
	    if (flagsOk && (2 < (dataLen - parsingAt)) &&
		(')' == data[parsingAt]) && (' ' == data[parsingAt+1])) {
		parsingAt += 2;
	    }
	    else {
		strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    }
	}
	else {
	    m_mailFlags = 0;
	}

	if ('"' == data[parsingAt]) {
	    ++parsingAt;
	    // The constructor for DateTime swallows both the leading and trailing
	    // double quote characters
	    if (messageDateTime.Parse(data, dataLen, parsingAt, DateTime::IMAP) &&
		('"' == data[parsingAt]) &&
		(' ' == data[parsingAt+1])) {
		parsingAt += 2;
	    }
	    else {
		strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    }
	}

	if ((2 < (dataLen - parsingAt)) && ('{' == data[parsingAt])) {
	    // It's a literal string
	    parsingAt++; // Skip the curly brace
	    char *close;
	    // read the number
	    m_literalLength = strtoul((char *)&data[parsingAt], &close, 10);
	    // check for the close curly brace and end of message and to see if I can fit all that data
	    // into the buffer
	    size_t lastChar = (size_t) (close - ((char *)data));
	    if ((NULL == close) || ('}' != close[0]) || (lastChar != (dataLen - 1))) {
		strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    }
	    else {
		std::string mailbox((char *)&m_parseBuffer[m_arguments]);
		
		switch (m_store->AddMessageToMailbox(mailbox, m_lineBuffer, 0, messageDateTime, m_mailFlags, &m_appendingUid)) {
		case MailStore::SUCCESS:
		    strncpy(m_responseText, "Ready for the Message Data", MAX_RESPONSE_STRING_LENGTH);
		    m_parseStage = 2;
		    result = IMAP_NOTDONE;
		    break;

		case MailStore::CANNOT_COMPLETE_ACTION:
		    result = IMAP_TRY_AGAIN;
		    break;

		default:
		    result = IMAP_MBOX_ERROR;
		    break;
		}
	    }
	}
	else {
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	}
    }
    else {
	strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
    }
    return result;
}

IMAP_RESULTS ImapSession::AppendHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    IMAP_RESULTS result = IMAP_OK;

    switch (m_parseStage) {
    case 0:
	switch (astring(data, dataLen, parsingAt, false, NULL)) {
	case ImapStringGood:
	    result = AppendHandlerExecute(data, dataLen, parsingAt);
	    break;

	case ImapStringBad:
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	    break;

	case ImapStringPending:
	    result = IMAP_NOTDONE;
	    m_parseStage = 1;
	    break;

	default:
	    strncpy(m_responseText, "Failed -- internal error", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_NO;
	    break;
	}
	break;

    case 1:
	// It's the mailbox name that's arrived
	if (dataLen >= m_literalLength) {
	    AddToParseBuffer(data, m_literalLength);
	    ++m_parseStage;
	    size_t i = m_literalLength;
	    m_literalLength = 0;
	    if (2 < dataLen) {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    m_responseText[0] = '\0';
	    result = AppendHandlerExecute(data, dataLen, i);
	}
	else {
	    result = IMAP_IN_LITERAL;
	    AddToParseBuffer(data, m_literalLength, false);
	    m_literalLength -= dataLen;
	}
	break;

    case 2:
	{
	    size_t residue;
	    // It's the message body that's arrived
	    size_t len = MIN(m_literalLength, dataLen);
	    if (m_literalLength < dataLen) {
		result = IMAP_IN_LITERAL;
		residue = dataLen - m_literalLength;
	    }
	    else {
		residue = 0;
	    }
	    std::string mailbox((char *)&m_parseBuffer[m_arguments]);
	    switch (m_store->AppendDataToMessage(mailbox, m_appendingUid, data, len)) {
	    case MailStore::SUCCESS:
		m_literalLength -= len;
		if (0 == m_literalLength) {
		    m_store->DoneAppendingDataToMessage(mailbox, m_appendingUid);
		    m_parseStage = 3;
		    m_appendingUid = 0;
		}
		break;

	    case MailStore::CANNOT_COMPLETE_ACTION:
		result = IMAP_TRY_AGAIN;
		break;

	    default:
		m_store->DoneAppendingDataToMessage(mailbox, m_appendingUid);
		m_store->DeleteMessage(mailbox, m_appendingUid);
		result = IMAP_MBOX_ERROR;
		m_appendingUid = 0;
		m_literalLength = 0;
		break;
	    }
	}
	break;

    default:
	strncpy(m_responseText, "Failed -- internal error", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_NO;
	break;
    }
    return result;
}

IMAP_RESULTS ImapSession::CheckHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    // Unlike NOOP, I always call MailboxFlushBuffers because that recalculates the the cached data.
    // That may be the only way to find out that the number of messages or the UIDNEXT value has
    // changed.
    IMAP_RESULTS result = IMAP_OK;
    NUMBER_SET purgedMessages;

    if (MailStore::SUCCESS == m_store->MailboxUpdateStats(&purgedMessages)) {
	m_purgedMessages.insert(purgedMessages.begin(), purgedMessages.end());
    }
    m_store->MailboxFlushBuffers();

    return result;
}

IMAP_RESULTS ImapSession::CloseHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    // If the mailbox is open, close it
    // In IMAP, deleted messages are always purged before a close
    NUMBER_SET purgedMessages;
    m_store->ListDeletedMessages(&purgedMessages);
    m_purgedMessages.insert(purgedMessages.begin(), purgedMessages.end());
    for(NUMBER_SET::iterator i=purgedMessages.begin(); i!=purgedMessages.end(); ++i) {
	m_store->ExpungeThisUid(*i);
    }
    m_store->MailboxClose();
    m_state = ImapAuthenticated;
    return IMAP_OK;
}

IMAP_RESULTS ImapSession::ExpungeHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
    IMAP_RESULTS result = IMAP_MBOX_ERROR;
    NUMBER_SET purgedMessages;

    switch (m_mboxErrorCode = m_store->ListDeletedMessages(&purgedMessages)) {
    case MailStore::SUCCESS:
	m_purgedMessages.insert(purgedMessages.begin(), purgedMessages.end());
	result = IMAP_OK;
	break;

    case MailStore::CANNOT_COMPLETE_ACTION:
	result = IMAP_TRY_AGAIN;
	break;
    }

    return result;
}

// A sequence set is a sequence of range values, each one of which is separated by commas
// A range value is one of a number, a star, or two of either of those separated by 
// a colon.  A star represents the last value in the range.  Two values separated by
// a colon represent the entire range of valid numbers between and including those two
// values.  A star is always valid unless the mail store has no messages in it.
//
// The resulting list of values must be sorted or nothing else works
bool ImapSession::MsnSequenceSet(SEARCH_RESULT &uidVector, size_t &tokenPointer) {
    bool result = true;

    size_t mboxCount = m_store->MailboxMessageCount();
    char *s, *e;
    s = (char *)&m_parseBuffer[tokenPointer];
    do {
	unsigned long first, second;
	if ('*' != s[0]) {
	    first = strtoul(s, &e, 10);
	}
	else {
	    first = mboxCount;
	    e = &s[1];
	}
	if (':' == *e) {
	    s = e+1;
	    if ('*' != s[0]) {
		second = strtoul(s, &e, 10);
	    }
	    else {
		second = mboxCount;
		e = &s[1];
	    }
	}
	else {
	    second = first;
	}
	if (second < first) {
	    unsigned long temp = first;
	    first = second;
	    second = temp;
	}
	if (second > mboxCount) {
	    second = mboxCount;
	}
	for (unsigned long i=first; i<=second; ++i) {
	    if (i <= mboxCount) {
		uidVector.push_back(m_store->MailboxMsnToUid(i));
	    }
	    else {
		result = false;
	    }
	}    
	s = e;
	if (',' == *s) {
	    ++s;
	}
	else {
	    if ('\0' != *s) {
		result = false;
	    }
	}
    } while (result && ('\0' != *s));
    return result;
}

bool ImapSession::UidSequenceSet(SEARCH_RESULT &uidVector, size_t &tokenPointer)
{
    bool result = true;

    size_t mboxCount = m_store->MailboxMessageCount();
    size_t mboxMaxUid;
    if (0 != mboxCount) {
	mboxMaxUid = m_store->MailboxMsnToUid(m_store->MailboxMessageCount());
    }
    else {
	mboxMaxUid = 0; 
    }
    size_t mboxMinUid;
    if (0 != mboxCount) {
	mboxMinUid = m_store->MailboxMsnToUid(1);
    }
    else {
	mboxMinUid = 0;
    }
    char *s, *e;
    s = (char *)&m_parseBuffer[tokenPointer];
    do
    {
	unsigned long first, second;
	if ('*' != s[0]) {
	    first = strtoul(s, &e, 10);
	}
	else {
	    first = mboxMaxUid;
	    e= &s[1];
	}
	if (':' == *e) {
	    s = e+1;
	    if ('*' != s[0]) {
		second = strtoul(s, &e, 10);
	    }
	    else {
		second = mboxMaxUid;
		e= &s[1];
	    }
	}
	else {
	    second = first;
	}
	if (second < first) {
	    unsigned long temp = first;
	    first = second;
	    second = temp;
	}
	if (first < mboxMinUid) {
	    first = mboxMinUid;
	}
	if (second > mboxMaxUid) {
	    second = mboxMaxUid;
	}
	for (unsigned long i=first; i<=second; ++i) {
	    if (0 < m_store->MailboxUidToMsn(i)) {
		uidVector.push_back(i);
	    }
	}    
	s = e;
	if (',' == *s) {
	    ++s;
	}
	else {
	    if ('\0' != *s) {
		result = false;
	    }
	}
    } while (result && ('\0' != *s));
    return result;
}

// UpdateSearchTerm updates the search specification for one term.  That term may be a 
// parenthesized list of other terms, in which case it calls UpdateSearchTerms to evaluate
// all of those.  UpdateSearchTerms will, in turn, call UpdateSearchTerm to update the 
// searchTerm for each of the terms in the list.
// UpdateSearchTerm return true if everything went okay or false if there were errors in
// the Search Specification String.
bool ImapSession::UpdateSearchTerm(MailSearch &searchTerm, size_t &tokenPointer) {
    bool result = true;

    if ('(' == m_parseBuffer[tokenPointer]) {
	tokenPointer += 2;
	result = UpdateSearchTerms(searchTerm, tokenPointer, true);
    }
    else {
	if (('*' == m_parseBuffer[tokenPointer]) || isdigit(m_parseBuffer[tokenPointer])) {
	    // It's a list of MSN ranges
	    // How do I treat a list of numbers?  Well, I generate a vector of all the possible values and then 
	    // and it (do a "set intersection" between it and the current vector, if any.

	    // I don't need to check to see if tokenPointer is less than m_parsePointer because this function
	    // wouldn't get called if it wasn't
	    SEARCH_RESULT srVector;
	    result = MsnSequenceSet(srVector, tokenPointer);
	    searchTerm.AddUidVector(srVector);
	    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	}
	else {
	    SEARCH_SYMBOL_T::iterator found = searchSymbolTable.find((char *)&m_parseBuffer[tokenPointer]);
	    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	    if (found != searchSymbolTable.end()) {
		switch (found->second) {
		case SSV_ERROR:
		    result = false;
		    break;

		case SSV_ALL:
		    // I don't do anything unless there are no other terms.  However, since there's an implicit "and all"
		    // around the whole expression, I don't have to do anything explicitly.
		    break;

		case SSV_ANSWERED:
		    searchTerm.AddBitsToIncludeMask(MailStore::IMAP_MESSAGE_ANSWERED);
		    break;

		case SSV_BCC:
		    if (tokenPointer < m_parsePointer) {
			searchTerm.AddBccSearch((char *)&m_parseBuffer[tokenPointer]);
			tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_BEFORE:
		    if (tokenPointer < m_parsePointer) {
			std::string dateString((char *)&m_parseBuffer[tokenPointer]);
			// SYZYGY -- need a try-catch here
			// DateTime date(dateString.c_str());
			DateTime date;
			// date.LooseDate(dateString);
			if (date.IsValid()) {
			    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
			    searchTerm.AddBeforeSearch(date);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_BODY:
		    if (tokenPointer < m_parsePointer) {
			searchTerm.AddBodySearch((char *)&m_parseBuffer[tokenPointer]);
			tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_CC:
		    if (tokenPointer < m_parsePointer) {
			searchTerm.AddCcSearch((char *)&m_parseBuffer[tokenPointer]);
			tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_DELETED:
		    searchTerm.AddBitsToIncludeMask(MailStore::IMAP_MESSAGE_DELETED);
		    break;

		case SSV_FLAGGED:
		    searchTerm.AddBitsToIncludeMask(MailStore::IMAP_MESSAGE_FLAGGED);
		    break;

		case SSV_FROM:
		    if (tokenPointer < m_parsePointer) {
			searchTerm.AddFromSearch((char *)&m_parseBuffer[tokenPointer]);
			tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_KEYWORD:
		    // Searching for this cannot return any matches because we don't have keywords to set
		    searchTerm.ForceNoMatches();
		    // However, I need to swallow the space and the atom that follows it if any, and it had 
		    // better or it's a syntax error
		    if (tokenPointer < m_parsePointer) {
			tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_NEW:
		    searchTerm.AddBitsToExcludeMask(MailStore::IMAP_MESSAGE_SEEN);
		    searchTerm.AddBitsToIncludeMask(MailStore::IMAP_MESSAGE_RECENT);
		    break;

		case SSV_OLD:
		    searchTerm.AddBitsToExcludeMask(MailStore::IMAP_MESSAGE_RECENT);
		    break;

		case SSV_ON:
		    if (tokenPointer < m_parsePointer) {
			std::string dateString((char *)&m_parseBuffer[tokenPointer]);
			// SYZYGY -- need a try-catch here
			// DateTime date(dateString.c_str()); 
			DateTime date;
			// date.LooseDate(dateString);
			if (date.IsValid()) {
			    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
			    searchTerm.AddOnSearch(date);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_RECENT:
		    searchTerm.AddBitsToIncludeMask(MailStore::IMAP_MESSAGE_RECENT);
		    break;

		case SSV_SEEN:
		    searchTerm.AddBitsToIncludeMask(MailStore::IMAP_MESSAGE_SEEN);
		    break;

		case SSV_SINCE:
		    if (tokenPointer < m_parsePointer) {
			std::string dateString((char *)&m_parseBuffer[tokenPointer]);	
			// SYZYGY -- need a try-catch here
			// DateTime date(dateString);
			DateTime date;
			// date.LooseDate(dateString);
			if (date.IsValid()) {
			    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
			    searchTerm.AddSinceSearch(date);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_SUBJECT:
		    if (tokenPointer < m_parsePointer) {
			searchTerm.AddSubjectSearch((char *)&m_parseBuffer[tokenPointer]);
			tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_TEXT:
		    if (tokenPointer < m_parsePointer) {
			searchTerm.AddTextSearch((char *)&m_parseBuffer[tokenPointer]);
			tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_TO:
		    if (tokenPointer < m_parsePointer) {
			searchTerm.AddToSearch((char *)&m_parseBuffer[tokenPointer]);
			tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_UNANSWERED:
		    searchTerm.AddBitsToExcludeMask(MailStore::IMAP_MESSAGE_ANSWERED);
		    break;

		case SSV_UNDELETED:
		    searchTerm.AddBitsToExcludeMask(MailStore::IMAP_MESSAGE_DELETED);
		    break;

		case SSV_UNFLAGGED:
		    searchTerm.AddBitsToExcludeMask(MailStore::IMAP_MESSAGE_FLAGGED);
		    break;

		case SSV_UNKEYWORD:
		    // Searching for this matches all messages because we don't have keywords to unset
		    // since all does nothing, this does nothing
		    // However, I need to swallow the space and the atom that follows it if any, and it had 
		    // better or it's a syntax error
		    if (tokenPointer < m_parsePointer) {
			tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_UNSEEN:
		    searchTerm.AddBitsToExcludeMask(MailStore::IMAP_MESSAGE_SEEN);
		    break;

		case SSV_DRAFT:
		    searchTerm.AddBitsToIncludeMask(MailStore::IMAP_MESSAGE_DRAFT);
		    break;

		case SSV_HEADER:
		    if (tokenPointer < m_parsePointer) {
			char *header_name = (char *)&m_parseBuffer[tokenPointer];
			tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
			if (tokenPointer < m_parsePointer) {
			    searchTerm.AddGenericHeaderSearch(header_name, (char *)&m_parseBuffer[tokenPointer]);
			    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_LARGER:
		    if (tokenPointer < m_parsePointer) {
			char *s;
			unsigned long value = strtoul((char *)&m_parseBuffer[tokenPointer], &s, 10);
			if ('\0' == *s) {
			    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
			    searchTerm.AddSmallestSize(value + 1);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_NOT:
		    // Okay, not works by evaluating the next term and then doing a symmetric set difference between
		    // that and the whole list of available messages.  The result is then treated as if it was a 
		    // list of numbers
		    if (tokenPointer < m_parsePointer) {
			MailSearch termToBeNotted;
			if (UpdateSearchTerm(termToBeNotted, tokenPointer)) {
			    SEARCH_RESULT vector;
			    // MessageList will not return CANNOT_COMPLETE here since I locked the mail store in the caller
			    if (MailStore::SUCCESS ==  m_store->MessageList(vector)) {
				if (MailStore::SUCCESS == termToBeNotted.Evaluate(m_store)) {
				    SEARCH_RESULT rightResult;
				    termToBeNotted.ReportResults(m_store, &rightResult);

				    for (SEARCH_RESULT::iterator i=rightResult.begin(); i!=rightResult.end(); ++i) {
					SEARCH_RESULT::iterator elem = find(vector.begin(), vector.end(), *i);
					if (vector.end() != elem) {
					    vector.erase(elem);
					}
				    }
				    searchTerm.AddUidVector(vector);
				}
				else {
				    result = false;
				}
			    }
			    else {
				result = false;
			    }
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_OR:
		    // Okay, or works by evaluating the next two terms and then doing a union between the two
		    // the result is then treated as if it is a list of numbers
		    if (tokenPointer < m_parsePointer)
		    {
			MailSearch termLeftSide;
			if (UpdateSearchTerm(termLeftSide, tokenPointer)) {
			    if (MailStore::SUCCESS == termLeftSide.Evaluate(m_store)) {
				MailSearch termRightSide;
				if (UpdateSearchTerm(termRightSide, tokenPointer)) {
				    if (MailStore::SUCCESS == termRightSide.Evaluate(m_store)) {
					SEARCH_RESULT leftResult;
					termLeftSide.ReportResults(m_store, &leftResult);
					SEARCH_RESULT rightResult;
					termRightSide.ReportResults(m_store, &rightResult);
					for (int i=0; i<rightResult.size(); ++i) {
					    unsigned long uid = rightResult[i];
					    int which;

					    if (leftResult.end() == find(leftResult.begin(), leftResult.end(), uid)) {
						leftResult.push_back(uid);
					    }
					}
					searchTerm.AddUidVector(leftResult);
				    }
				    else {
					result = false;
				    }
				}
				else {
				    result = false;
				}
			    }
			    else {
				result = false;
			    }
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_SENTBEFORE:
		    if (tokenPointer < m_parsePointer) {
			std::string dateString((char *)&m_parseBuffer[tokenPointer]);
			// SYZYGY -- need a try-catch here
			// DateTime date(dateString);
			DateTime date;
			// date.LooseDate(dateString);
			if (date.IsValid()) {
			    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
			    searchTerm.AddSentBeforeSearch(date);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_SENTON:
		    if (tokenPointer < m_parsePointer) {
			std::string dateString((char *)&m_parseBuffer[tokenPointer]);
			// SYZYGY -- need a try-catch here
			// DateTime date(dateString);
			DateTime date;
			// date.LooseDate(dateString);
			if (date.IsValid()) {
			    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
			    searchTerm.AddSentOnSearch(date);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_SENTSINCE:
		    if (tokenPointer < m_parsePointer) {
			std::string dateString((char *)&m_parseBuffer[tokenPointer]);
			// SYZYGY -- need a try-catch here
			// DateTime date(dateString);
			DateTime date;
			// date.LooseDate(dateString);
			if (date.IsValid()) {
			    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
			    searchTerm.AddSentSinceSearch(date);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_SMALLER:
		    if (tokenPointer < m_parsePointer) {
			char *s;
			unsigned long value = strtoul((char *)&m_parseBuffer[tokenPointer], &s, 10);
			if ('\0' == *s) {
			    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
			    searchTerm.AddLargestSize(value - 1);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_UID:
		    // What follows is a UID list
		    if (tokenPointer < m_parsePointer) {
			SEARCH_RESULT srVector;

			result = UidSequenceSet(srVector, tokenPointer);
			searchTerm.AddUidVector(srVector);
			tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_UNDRAFT:
		    searchTerm.AddBitsToExcludeMask(MailStore::IMAP_MESSAGE_DRAFT);
		    break;

		default:
		    result = false;
		    break;
		}
	    }
	    else {
		result = false;
	    }
	}
    }
    return result;
}

// UpdateSearchTerms calls UpdateSearchTerm until there is an error, it reaches a ")" or it reaches the end of 
// the string.  If there is an error, indicated by UpdateSearchTerm returning false, or if it reaches a ")" and
// isSubExpression is not true, then false is returned.  Otherwise, true is returned.  
bool ImapSession::UpdateSearchTerms(MailSearch &searchTerm, size_t &tokenPointer, bool isSubExpression) {
    bool notdone = true;
    bool result;
    do {
	result = UpdateSearchTerm(searchTerm, tokenPointer);
    } while(result && (tokenPointer < m_parsePointer) && (')' != m_parseBuffer[tokenPointer]));
    if ((')' == m_parseBuffer[tokenPointer]) && !isSubExpression) {
	result = false;
    }
    if (')' == m_parseBuffer[tokenPointer]) {
	tokenPointer += 2;
    }
    return result;
}

// SearchHandlerExecute assumes that the tokens for the search are in m_parseBuffer from 0 to m_parsePointer and
// that they are stored there as a sequence of ASCIIZ strings.  It is also known that when the system gets
// here that it's time to generate the output
IMAP_RESULTS ImapSession::SearchHandlerExecute(bool usingUid) {
    SEARCH_RESULT foundVector;
    NUMBER_LIST foundList;
    IMAP_RESULTS result;
    MailSearch searchTerm;
    bool hasNoMatches = false;
    size_t executePointer = 0;

    if (MailStore::SUCCESS == m_store->MailboxLock()) {
	// Skip the first two tokens that are the tag and the command
	executePointer += (size_t) strlen((char *)m_parseBuffer) + 1;
	executePointer += (size_t) strlen((char *)&m_parseBuffer[executePointer]) + 1;

	// I do this once again if bUsingUid is set because the command in that case is UID
	if (usingUid) {
	    executePointer += (size_t) strlen((char *)&m_parseBuffer[executePointer]) + 1;
	}

	// This section turns the search spec into something that might work efficiently
	if (UpdateSearchTerms(searchTerm, executePointer, false)) {
	    m_mboxErrorCode = searchTerm.Evaluate(m_store);
	    searchTerm.ReportResults(m_store, &foundVector);

	    // This section turns the vector of UID numbers into a list of either UID numbers or MSN numbers
	    // whichever I'm supposed to be displaying.  If I were putting the numbers into some sort of 
	    // order, this is where that would happen
	    if (!usingUid) {
		for (int i=0; i<foundVector.size(); ++i) {
		    if (0 != foundVector[i]) {
			foundList.push_back(m_store->MailboxUidToMsn(foundVector[i]));
		    }
		}
	    }
	    else {
		for (int i=0; i<foundVector.size(); ++i) {
		    if (0 != foundVector[i]) {
			foundList.push_back(foundVector[i]);
		    }
		}
	    }
	    // This section writes the generated list of numbers
	    if (MailStore::SUCCESS == m_mboxErrorCode) {
		std::ostringstream ss;
		ss << "* SEARCH";
		for(NUMBER_LIST::iterator i=foundList.begin(); i!=foundList.end(); ++i) {
		    ss << " " << *i;
		}
		ss << "\r\n";
		m_s->Send((uint8_t *)ss.str().c_str(), ss.str().size());
		result = IMAP_OK;
	    }
	    else {
		result = IMAP_MBOX_ERROR;
	    }
	}
	else {
	    result = IMAP_BAD;
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	}
	m_store->MailboxUnlock();
    }
    else {
	result = IMAP_TRY_AGAIN;
    }

    return result;
}

IMAP_RESULTS ImapSession::SearchKeyParse(uint8_t *data, size_t dataLen, size_t &parsingAt) {
    IMAP_RESULTS result = IMAP_OK;
    while((IMAP_OK == result) && (dataLen > parsingAt)) {
	if (' ' == data[parsingAt]) {
	    ++parsingAt;
	}
	if ('(' == data[parsingAt]) {
	    AddToParseBuffer(&data[parsingAt], 1);
	    ++parsingAt;
	    if (('*' == data[parsingAt]) || isdigit(data[parsingAt])) {
		char *end1 = strchr(((char *)data)+parsingAt, ' ');
		char *end2 = strchr(((char *)data)+parsingAt, ')');
		size_t len;
		if (NULL != end2) {
		    if ((NULL == end1) || (end2 < end1)) {
			len = (end2 - (((char *)data)+parsingAt));
		    }
		    else {
			len = (end1 - (((char *)data)+parsingAt));
		    }
		}
		else {
		    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		}
		if ((0 < len) && (dataLen >= (parsingAt + len))) {
		    AddToParseBuffer(&data[parsingAt], len);
		    parsingAt += len;
		}
		else {
		    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		}
	    }
	    else {
		switch(astring(data, dataLen, parsingAt, true, NULL)) {
		case ImapStringGood:
		    break;

		case ImapStringBad:
		    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;

		case ImapStringPending:
		    strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_NOTDONE;
		    m_parseStage = 1;
		    break;

		default:
		    strncpy(m_responseText, "Internal Search Error", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;
		}
	    }
	}
	else {
	    if (('*' == data[parsingAt]) || isdigit(data[parsingAt])) {
		char *end1 = strchr(((char *)data)+parsingAt, ' ');
		char *end2 = strchr(((char *)data)+parsingAt, ')');
		size_t len;
		if (NULL != end2) {
		    if ((NULL == end1) || (end2 < end1)) {
			len = (end2 - (((char *)data)+parsingAt));
		    }
		    else {
			len = (end1 - (((char *)data)+parsingAt));
		    }
		}
		else {
		    if (NULL != end1) {
			len = (end1 - (((char *)data)+parsingAt));
		    }
		    else {
			len = strlen(((char *)data)+parsingAt);
		    }
		}
		if ((0 < len) && (dataLen >= (parsingAt + len))) {
		    AddToParseBuffer(&data[parsingAt], len);
		    parsingAt += len;
		}
		else {
		    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		}
	    }
	    else {
		switch(astring(data, dataLen, parsingAt, true, "*")) {
		case ImapStringGood:
		    break;

		case ImapStringBad:
		    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;

		case ImapStringPending:
		    strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_NOTDONE;
		    m_parseStage = 1;
		    break;

		default:
		    strncpy(m_responseText, "Internal Search Error", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;
		}
	    }
	}
	if ((dataLen >= parsingAt)  && (')' == data[parsingAt])) {
	    AddToParseBuffer(&data[parsingAt], 1);
	    ++parsingAt;
	}
    }
    if (IMAP_OK == result) {
	m_parseStage = 3;
	result = SearchHandlerExecute(m_usesUid);
    }
    return result;
}


IMAP_RESULTS ImapSession::SearchHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool usingUid) {
    IMAP_RESULTS result = IMAP_OK;

    m_usesUid = usingUid;
    size_t firstArgOffset = m_parsePointer;
    size_t savedParsePointer = parsingAt;
    switch (m_parseStage) {
    case 0:
	if (('(' == data[parsingAt]) || ('*' == data[parsingAt])) {
	    m_parsePointer = firstArgOffset;
	    parsingAt = savedParsePointer;
	    result = SearchKeyParse(data, dataLen, parsingAt);
	}
	else {
	    atom(data, dataLen, parsingAt);
	    if (0 == strcmp("CHARSET", (char *)&m_parseBuffer[firstArgOffset])) {
		if ((2 < (dataLen - parsingAt)) && (' ' == data[parsingAt])) {
		    ++parsingAt;
		    m_parsePointer = firstArgOffset;
		    switch (astring(data, dataLen, parsingAt, true, NULL)) {
		    case ImapStringGood:
			// I have the charset astring--check to make sure it's "US-ASCII"
			// for the time being, it's the only charset I recognize
			if (0 == strcmp("US-ASCII", (char *)&m_parseBuffer[firstArgOffset])) {
			    m_parseStage = 2;
			    m_parsePointer = firstArgOffset;
			    result = SearchKeyParse(data, dataLen, parsingAt);
			}
			else {
			    strncpy(m_responseText, "Unrecognized Charset", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			}
			break;

		    case ImapStringBad:
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
			break;

		    case ImapStringPending:
			result = IMAP_NOTDONE;
			m_parseStage = 1;
			strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
			break;

		    default:
			strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
			break;
		    }
		}
		else {
		    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		}
	    }
	    else {
		m_parsePointer = firstArgOffset;
		parsingAt = savedParsePointer;
		result = SearchKeyParse(data, dataLen, parsingAt);
	    }
	}
	break;

    case 1:
	strncpy(m_responseText, "Completed", MAX_RESPONSE_STRING_LENGTH);
	// It was receiving the charset astring
	// for the time being, it's the only charset I recognize
	if ((dataLen > m_literalLength) && (8 == m_literalLength)) {
	    for (int i=0; i<8; ++i) {
		data[i] = toupper(data[i]);
	    }
	    if (11 < dataLen) {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    if (0 == strncmp("US-ASCII ", (char *)data, 9)) {
		size_t i = 9;
		result = SearchKeyParse(data, dataLen, i);
	    }
	    else {
		strncpy(m_responseText, "Unrecognized Charset", MAX_RESPONSE_STRING_LENGTH);
		result = IMAP_BAD;
	    }
	}
	else {
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
	break;

    case 2:
	strncpy(m_responseText, "Completed", MAX_RESPONSE_STRING_LENGTH);
	// It's somewhere in the middle of the search specification
	if (dataLen >= m_literalLength) {
	    AddToParseBuffer(data, m_literalLength);
	    size_t i = m_literalLength;
	    m_literalLength = 0;
	    if (2 < dataLen) {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    result = SearchKeyParse(data, dataLen, i);
	}
	else {
	    result = IMAP_IN_LITERAL;
	    AddToParseBuffer(data, dataLen, false);
	    m_literalLength -= dataLen;
	}
	break;

    default:
	strncpy(m_responseText, "Completed", MAX_RESPONSE_STRING_LENGTH);
	result = SearchHandlerExecute(m_usesUid);
	break;
    }
    return result;
}

// RemoveRfc822Comments assumes that the lines have already been unfolded
static insensitiveString RemoveRfc822Comments(const insensitiveString &headerLine) {
    insensitiveString result;
    bool inComment = false;
    bool inQuotedString = false;
    bool escapeFlag = false;
    int len = headerLine.size();

    for(int i=0; i<len; ++i) {
	if (inComment) {
	    if (escapeFlag) {
		escapeFlag = false;
	    }
	    else {
		if (')' == headerLine[i]) {
		    inComment = false;
		}
		else {
		    if ('\\' == headerLine[i]) {
			escapeFlag = true;
		    }
		}
	    }
	}
	else {
	    if (inQuotedString) {
		if(escapeFlag) {
		    escapeFlag = false;
		}
		else {
		    if ('"' == headerLine[i]) {
			inQuotedString = false;
		    }
		    else {
			if ('\\' == headerLine[i]) {
			    escapeFlag = true;
			}
		    }
		}
		result += headerLine[i];
	    }
	    else {
		// Look for quote and paren
		if ('(' == headerLine[i]) {
		    inComment = true;
		}
		else {
		    if ('"' == headerLine[i]) {
			inQuotedString = true;
		    }
		    result += headerLine[i];
		}
	    }
	}
    }
    return result;
}

void ImapSession::SendMessageChunk(unsigned long uid, size_t offset, size_t length) {
    if (MailStore::SUCCESS == m_store->OpenMessageFile(uid)) {
	char *xmitBuffer = new char[length+1];
	size_t charsRead = m_store->ReadMessage(xmitBuffer, offset, length);
	m_store->CloseMessageFile();
	xmitBuffer[charsRead] = '\0';

	std::ostringstream literal;
	literal << "{" << charsRead << "}\r\n";
	m_s->Send((uint8_t *)literal.str().c_str(), literal.str().size());
	m_s->Send((uint8_t *)xmitBuffer, charsRead);
	delete[] xmitBuffer;
    }
}

void ImapSession::FetchResponseFlags(uint32_t flags) {
    std::string result("FLAGS ("), separator;
    if (MailStore::IMAP_MESSAGE_SEEN & flags) {
	result += "\\Seen";
	separator = " ";
    }
    if (MailStore::IMAP_MESSAGE_ANSWERED & flags) {
	result += separator;
	result += "\\Answered";
	separator = " ";
    }
    if (MailStore::IMAP_MESSAGE_FLAGGED & flags)  {
	result += separator;
	result += "\\Flagged";
	separator = " ";
    }
    if (MailStore::IMAP_MESSAGE_DELETED & flags) {
	result += separator;
	result += "\\Deleted";
	separator = " ";
    }
    if (MailStore::IMAP_MESSAGE_DRAFT & flags) {
	result += separator;
	result += "\\Draft";
	separator = " ";
    }
    if (MailStore::IMAP_MESSAGE_RECENT & flags) {
	result += separator;
	result += "\\Recent";
    }
    result += ")";
    m_s->Send((uint8_t *) result.c_str(), result.size());
}

void ImapSession::FetchResponseInternalDate(const MailMessage *message) {
    std::string result = "INTERNALDATE \"";
    DateTime when = m_store->MessageInternalDate(message->GetUid());
    when.SetFormat(DateTime::IMAP);

    result += when.str();
    result += "\"";
    m_s->Send((uint8_t *)result.c_str(), result.size());
}

void ImapSession::FetchResponseRfc822(unsigned long uid, const MailMessage *message) {
    m_s->Send((uint8_t *) "RFC822 ", 7);
    SendMessageChunk(uid, 0, message->GetMessageBody().bodyOctets);
}

void ImapSession::FetchResponseRfc822Header(unsigned long uid, const MailMessage *message) {
    size_t length;
    if (0 != message->GetMessageBody().headerOctets) {
	length = message->GetMessageBody().headerOctets;
    }
    else {
	length = message->GetMessageBody().bodyOctets;
    }
    m_s->Send((uint8_t *) "RFC822.HEADER ", 14);
    SendMessageChunk(uid, 0, length);
}

void ImapSession::FetchResponseRfc822Size(const MailMessage *message) {
    std::ostringstream result;
    result << "RFC822.SIZE " << message->GetMessageBody().bodyOctets;
    m_s->Send((uint8_t *) result.str().c_str(), result.str().size());
}

void ImapSession::FetchResponseRfc822Text(unsigned long uid, const MailMessage *message) {
    std::string result;
    if (0 < message->GetMessageBody().headerOctets) {
	size_t length;
	if (message->GetMessageBody().bodyOctets > message->GetMessageBody().headerOctets) {
	    length = message->GetMessageBody().bodyOctets - message->GetMessageBody().headerOctets;
	}
	else {
	    length = 0;
	}
	m_s->Send((uint8_t *) "RFC822.TEXT ", 12);
	SendMessageChunk(uid, message->GetMessageBody().headerOctets, length);
    }
    else {
	m_s->Send((uint8_t *) "RFC822.TEXT {0}\r\n", 17);
    }
}

static insensitiveString QuotifyString(const insensitiveString &input) {
    insensitiveString result("\"");

    for (int i=0; i<input.size(); ++i) {
	if (('"' == input[i]) || ('\\' == input[i])) {
	    result += '\\';
	}
	result += input[i];
    }
    result += "\"";

    return result;
}


#define SPACE " \t\v\r\n\f"

static insensitiveString Rfc822DotAtom(insensitiveString &input)
{
    insensitiveString result;
    bool notdone = true;

    int begin = input.find_first_not_of(SPACE);
    int end = input.find_last_not_of(SPACE);
    if (std::string::npos != begin) {
	input = input.substr(begin, end-begin+1);
    }
    else {
	input = "";
    }

    if ('"' == input[0]) {
	input = input.substr(1);
	bool escapeFlag = false;
	int i;
	for (i=0; i<input.size(); ++i) {
	    if (escapeFlag) {
		escapeFlag = false;
		result += input[i];
	    }
	    else {
		if ('"' == input[i]) {
		    break;
		}
		else {
		    if ('\\' == input[i]) {
			escapeFlag = true;
		    }
		    else {
			result += input[i];
		    }
		}
	    }
	}
	input = input.substr(i+1);
    }
    else {
	int pos = input.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&'*+-/=?^_`{|}~. ");
	if (std::string::npos != pos) {
	    int begin = input.find_first_not_of(SPACE);
	    int end = input.find_last_not_of(SPACE, pos-1);
	    result = input.substr(begin, end-begin+1);
	    input = input.substr(pos);
	}
	else {
	    result = input;
	    input = "";
	}
    }
    return result;
}


static insensitiveString ParseRfc822Domain(insensitiveString &input) {
    insensitiveString result;

    if ('[' == input[1]) {
	input = input.substr(1);
	std::string token;
	bool escapeFlag = false;
	bool notdone = true;
	int i;
	for (i=0; (i<input.size()) && notdone; ++i) {
	    if (escapeFlag) {
		token += input[i];
		escapeFlag = false;
	    }
	    else {
		if ('\\' == input[i]) {
		    escapeFlag = true;
		}
		else {
		    if (']' == input[i])  {
			notdone = false;
		    }
		    else {
			token += input[i];
		    }
		}
	    }
	}
	input = input.substr(i+1);
    }
    else {
	insensitiveString token = Rfc822DotAtom(input);
	result += QuotifyString(token);
    }
    return result;
}

static insensitiveString ParseRfc822AddrSpec(insensitiveString &input) {
    insensitiveString result;

    if ('@' != input[0]) {
	result += "NIL ";
    }
    else {
	// It's an old literal path
	int pos = input.find(':');
	if (std::string::npos != pos) {
	    insensitiveString temp = QuotifyString(input.substr(0, pos));
	    int begin = temp.find_first_not_of(SPACE);
 	    int end = temp.find_last_not_of(SPACE, pos-1);
	    result += temp.substr(begin, end-begin+1) + " ";
	    input = input.substr(pos+1);
	}
	else {
	    result += "NIL ";
	}
    }
    insensitiveString token = Rfc822DotAtom(input);
    result += QuotifyString(token) + " ";
    if ('@' == input[0]) {
	input = input.substr(1);
	result += ParseRfc822Domain(input);
    }
    else {
	result += "\"\"";
    }
    return result;
}

static insensitiveString ParseMailbox(insensitiveString &input) {
    insensitiveString result("(");
    int begin = input.find_first_not_of(SPACE);
    int end = input.find_last_not_of(SPACE);
    input = input.substr(begin, end-begin+1);
    if ('<' == input[0]) {
	result += "NIL ";

	begin = input.find_first_not_of(SPACE, 1);
	end = input.find_last_not_of(SPACE);
	input = input.substr(begin, end-begin+1);
	// Okay, this is an easy one.  This is an "angle-addr"
	result += ParseRfc822AddrSpec(input);
	begin = input.find_first_not_of(SPACE);
	end = input.find_last_not_of(SPACE);
	input = input.substr(begin, end-begin+1);
	if ('>' == input[0]) {
	    begin = input.find_first_not_of(SPACE, 1);
	    end = input.find_last_not_of(SPACE);
	    input = input.substr(begin, end-begin+1);
	}
    }
    else {
	if ('@' == input[0]) {
	    result += "NIL " + ParseRfc822AddrSpec(input);
	}
	else {
	    insensitiveString token = Rfc822DotAtom(input);
	    begin = input.find_first_not_of(SPACE);
	    end = input.find_last_not_of(SPACE);
	    input = input.substr(begin, end-begin+1);

	    // At this point, I don't know if I've seen a local part or an address_spec
	    // I can tell the difference by the first character.  If it's a at sign or if I'm out of string,
	    // then it's a local part  If it's a greater than, then it was a display name and I'm expecting
	    // an address
	    if ((0 == input.size()) || (',' == input[0])) {
		result += "NIL NIL " + QuotifyString(token) + " \"\"";
	    }
	    else {
		if ('@' == input[0]) {
		    input = input.substr(1);
		    result += "NIL NIL " + QuotifyString(token) + " " + ParseRfc822Domain(input);
		}
		else {
		    if ('<' == input[0]) {
			input = input.substr(1);
		    }
		    result += QuotifyString(token) + " " + ParseRfc822AddrSpec(input);
		}
	    }
	}
    }
    result += ")";
    return result;
}

static insensitiveString ParseMailboxList(const insensitiveString &input) {
    insensitiveString result;

    if (0 != input.size()) {
	result = "(";
	insensitiveString work = RemoveRfc822Comments(input);
	do {
	    if (',' == work[0]) {
		work = work.substr(1);
	    }
	    result += ParseMailbox(work);
	    int end = work.find_last_not_of(SPACE);
	    int begin = work.find_first_not_of(SPACE);
	    work = work.substr(begin, end-begin+1);
	} while (',' == work[0]);
	result += ")";
    }
    else {
	result = "NIL";
    }
    return result;
}

// An address is either a mailbox or a group.  A group is a display-name followed by a colon followed
// by a mailbox list followed by a semicolon.  A mailbox is either an address specification or a name-addr.
//  An address specification is a mailbox followed by a 
static insensitiveString ParseAddress(insensitiveString &input) {
    insensitiveString result("(");
    int end = input.find_last_not_of(SPACE);
    int begin = input.find_first_not_of(SPACE);
    input = input.substr(begin, end-begin+1);
    if ('<' == input[0]) {
	result += "NIL ";

	end = input.find_last_not_of(SPACE);
	begin = input.find_first_not_of(SPACE, 1);
	input = input.substr(begin, end-begin+1);
	// Okay, this is an easy one.  This is an "angle-addr"
	result += ParseRfc822AddrSpec(input);
	end = input.find_last_not_of(SPACE);
	begin = input.find_first_not_of(SPACE);
	input = input.substr(begin, end-begin+1);
	if ('>' == input[0]) {
	    end = input.find_last_not_of(SPACE);
	    begin = input.find_first_not_of(SPACE, 1);
	    if (std::string::npos == begin) {
		begin = 0;
	    }
	    input = input.substr(begin, end-begin+1);
	}
    }
    else {
	if ('@' == input[0]) {
	    result += "NIL " + ParseRfc822AddrSpec(input);
	}
	else {
	    insensitiveString token = Rfc822DotAtom(input);

	    end = input.find_last_not_of(SPACE);
	    begin = input.find_first_not_of(SPACE);
	    input = input.substr(begin, end-begin+1);

	    // At this point, I don't know if I've seen a local part, a display name, or an address_spec
	    // I can tell the difference by the first character.  If it's a at sign or if I'm out of string,
	    // then it's a local part  If it's a colon, then it was a display name and I'm reading a group.
	    //  If it's a greater than, then it was a display name and I'm expecting an address
	    if ((0 == input.size()) || (',' == input[0])) {
		result += "NIL NIL " + QuotifyString(token) + " \"\"";
	    }
	    else {
		if ('@' == input[0]) {
		    input = input.substr(1);
		    insensitiveString domain = ParseRfc822Domain(input);
		    if ('(' == input[0]) {
			input = input.substr(1, input.find(')')-1);
			result += QuotifyString(input) + " NIL " + QuotifyString(token) + " " + domain;
		    }
		    else {
			result += "NIL NIL " + QuotifyString(token) + " " + domain;
		    }
		    // SYZYGY -- 
		    // SYZYGY -- I wonder what I forgot in the previous line
		}
		else {
		    if ('<' == input[0]) {
			input = input.substr(1);
			result += QuotifyString(token) + " " + ParseRfc822AddrSpec(input);
		    }
		    else {
			if (':' == input[0]) {
			    input = input.substr(1);
			    // It's a group specification.  I mark the start of the group with (NIL NIL <token> NIL)
			    result += "NIL NIL " + QuotifyString(token) + " NIL)";
			    // The middle is a mailbox list
			    result += ParseMailboxList(input);
			    // I mark the end of the group with (NIL NIL NIL NIL)
			    if (';' == input[0]) {
				input = input.substr(1);
				result += "(NIL NIL NIL NIL";
			    }
			}
			else {
			    result += QuotifyString(token) + " " + ParseRfc822AddrSpec(input);
			}
		    }
		}
	    }
	}
    }
    result += ")";
    return result;
}

static insensitiveString ParseAddressList(const insensitiveString &input) {
    insensitiveString result;

    if (0 != input.size()) {
	result = "(";
	insensitiveString work = input; // RemoveRfc822Comments(input);
	do {
	    if (',' == work[0]) {
		work = work.substr(1);
	    }
	    result += ParseAddress(work);
	    int begin = work.find_first_not_of(SPACE);
	    if (std::string::npos != begin) {
		int end = work.find_last_not_of(SPACE);
		work = work.substr(begin, end-begin+1);
	    }
	    else {
		work = "";
	    }
	} while (',' == work[0]);
	result += ")";
    }
    else {
	result = "NIL";
    }
    return result;
}

// FetchResponseEnvelope returns the envelope data of the message.
// The envelope data is a parenthesized list with the following fields
// in this order:
// date (string -- taken from m_csDateLine), subject (string -- taken from m_csSubject),
// from (parenthesized list of addresses from m_csFromLine), sender (parenthesized list
// of addresses from m_csSenderLine), reply-to (parenthesized list of addresses from 
// m_csReplyToLine), to (parenthesized list of addresses from m_csToLine), cc (parenthesized
// list of addresses from m_csCcLine), bcc (parenthesized list of addresses from m_csBccLine)
// in-reply-to (string from m_csInReplyTo), and message-id (string from m_csMessageId)
void ImapSession::FetchResponseEnvelope(const MailMessage *message) {
    insensitiveString result("ENVELOPE ("); 
    result += QuotifyString(message->GetDateLine()) + " ";
    if (0 != message->GetSubject().size()) {
	result += QuotifyString(message->GetSubject()) + " ";
    }
    else {
	result += "NIL ";
    }
    insensitiveString from = ParseAddressList(message->GetFrom()) + " ";
    result += from;
    if (0 != message->GetSender().size()) {
	result += ParseAddressList(message->GetSender()) + " ";
    }
    else {
	result += from;
    }
    if (0 != message->GetReplyTo().size()) {
        result += ParseAddressList(message->GetReplyTo()) + " ";
    }
    else {
	result += from;
    }
    result += ParseAddressList(message->GetTo()) + " ";
    result += ParseAddressList(message->GetCc()) + " ";
    result += ParseAddressList(message->GetBcc()) + " ";
    if (0 != message->GetInReplyTo().size()) {
	result += QuotifyString(message->GetInReplyTo()) + " ";
    }
    else
    {
	result += "NIL ";
    }
    if (0 != message->GetMessageId().size()) {
	result += QuotifyString(message->GetMessageId());
    }
    else {
	result += "NIL ";
    }
    result += ")";
    m_s->Send((uint8_t *)result.c_str(), result.size());
}

// The part before the slash is the body type
// The default type is "TEXT"
static insensitiveString ParseBodyType(const insensitiveString &typeLine) {	
    insensitiveString result = RemoveRfc822Comments(typeLine);
    if (0 == result.size()) {
	result = "\"TEXT\"";
    }
    else {
	int pos = result.find('/');
	if (std::string::npos != pos) {
	    result = result.substr(0, pos);
	}
	else {
	    pos = result.find(';');
	    if (std::string::npos != pos) {
		result = result.substr(0, pos);
	    }
	}
	int end = result.find_last_not_of(SPACE);
	int begin = result.find_first_not_of(SPACE);
	result = result.substr(begin, end-begin+1);
	result = "\"" + result + "\"";
    }
    return result;
}

// the part between the slash and the semicolon (if any) is the body subtype
// The default subtype is "PLAIN" for text types and "UNKNOWN" for others
static insensitiveString ParseBodySubtype(const insensitiveString &typeLine, MIME_MEDIA_TYPES type) {
    insensitiveString result = RemoveRfc822Comments(typeLine);
    int pos = result.find('/');
    if (std::string::npos == pos) {
	if (MIME_TYPE_TEXT == type) {
	    result = "\"PLAIN\"";
	}
	else {
	    result = "\"UNKNOWN\"";
	}
    }
    else {
	result = result.substr(pos+1);
	pos = result.find(';');
	if (std::string::npos != pos) {
	    result = result.substr(0, pos);
	}
	int end = result.find_last_not_of(SPACE);
	int begin = result.find_first_not_of(SPACE);
	result = result.substr(begin, end-begin+1);
	result = "\"" + result + "\"";
    }
    return result;
}

// The parameters are all after the first semicolon
// The default parameters are ("CHARSET" "US-ASCII") for text types, and
// NIL for everything else
static insensitiveString ParseBodyParameters(const insensitiveString &typeLine, MIME_MEDIA_TYPES type) {
    insensitiveString uncommented = RemoveRfc822Comments(typeLine);
    insensitiveString result;
	
    int pos = uncommented.find(';'); 
    if (std::string::npos == pos) {
	if (MIME_TYPE_TEXT == type) {
	    result = "(\"CHARSET\" \"US-ASCII\")";
	}
	else {
	    result = "NIL";
	}
    }
    else {
	insensitiveString residue = uncommented.substr(pos+1);
	bool inQuotedString = false;
	bool escapeFlag = false;

	result = "(\"";
	for (int pos=0; pos<residue.size(); ++pos) {
	    if (inQuotedString) {
		if (escapeFlag) {
		    escapeFlag = false;
		    result += residue[pos];
		}
		else {
		    if ('"' == residue[pos]) {
			inQuotedString = false;
		    }
		    else {
			if ('\\' == residue[pos]) {
			    escapeFlag = true;
			}
			else {
			    result += residue[pos];
			}
		    }
		}
	    }
	    else {
		if (' ' != residue[pos]) {
		    if ('"' == residue[pos]) {
			inQuotedString = true;
		    }
		    else {
			if (';' == residue[pos]) {
			    result += "\" \"";
			}
			else {
			    if ('=' == residue[pos]) {
				result += "\" \"";
			    }
			    else {
				result += residue[pos];
			    }
			}
		    }
		}
	    }
	}
	result += "\")";
    }
    return result;
}

// FetchSubpartEnvelope returns the envelope data of an RFC 822 message subpart.
// The envelope data is a parenthesized list with the following fields
// in this order:
// date (string -- taken from m_csDateLine), subject (string -- taken from m_subject),
// from (parenthesized list of addresses from m_fromLine), sender (parenthesized list
// of addresses from m_senderLine), reply-to (parenthesized list of addresses from
// m_csReplyToLine), to (parenthesized list of addresses from m_csToLine), cc (parenthesized
// list of addresses from m_csCcLine), bcc (parenthesized list of addresses from m_csBccLine)
// in-reply-to (string from m_csInReplyTo), and message-id (string from m_csMessageId)
static insensitiveString FetchSubpartEnvelope(const MESSAGE_BODY &body)
{
    insensitiveString result("(");
    HEADER_FIELDS::const_iterator field = body.fieldList.find("date");
    if (body.fieldList.end() != field) {
	result += QuotifyString(field->second.c_str()) + " ";
    }
    else {
	result += "NIL ";
    }
    field = body.fieldList.find("subject");
    if (body.fieldList.end() != field) {
	result += QuotifyString(field->second.c_str()) + " ";
    }
    else {
	result += "NIL ";
    }
    field = body.fieldList.find("from");
    insensitiveString from;
    if (body.fieldList.end() != field)
    {
	from = ParseAddressList(field->second.c_str()) + " ";
    }
    else
    {
	from = "NIL ";
    }
    result += from;
    field = body.fieldList.find("sender");
    if (body.fieldList.end() != field) {
	result += ParseAddressList(field->second.c_str()) + " ";
    }
    else {
	result += from;
    }
    field = body.fieldList.find("reply-to");
    if (body.fieldList.end() != field) {
        result += ParseAddressList(field->second.c_str()) + " ";
    }
    else {
	result += from;
    }
    field = body.fieldList.find("to");
    if (body.fieldList.end() != field) {
	result += ParseAddressList(field->second.c_str()) + " ";
    }
    else {
	result += "NIL ";
    }
    field = body.fieldList.find("cc");
    if (body.fieldList.end() != field) {
	result += ParseAddressList(field->second.c_str()) + " ";
    }
    else {
	result += "NIL ";
    }
    field = body.fieldList.find("bcc");
    if (body.fieldList.end() != field) {
	result += ParseAddressList(field->second.c_str()) + " ";
    }
    else {
	result += "NIL ";
    }
    field = body.fieldList.find("in-reply-to");
    if (body.fieldList.end() != field) {
	result += QuotifyString(field->second.c_str()) + " ";
    }
    else {
	result += "NIL ";
    }
    field = body.fieldList.find("message-id");
    if (body.fieldList.end() != field) {
	result += QuotifyString(field->second.c_str());
    }
    else {
	result += "NIL";
    }
    result += ")";
    return result;
}

// FetchResponseBodyStructure builds a bodystructure response.  The body structure is a representation
// of the MIME structure of the message.  It starts at the main part and goes from there.
// The results for each part are a parenthesized list of values.  For non-multipart parts, the
// fields are the body type (derived from the contentTypeLine), the body subtype (derived from the
// contentTypeLine), the body parameter parenthesized list (derived from the contentTypeLine), the
// body id (from contentIdLine), the body description (from contentDescriptionLine), the body
// encoding (from contentEncodingLine) and the body size (from text.GetLength()).  Text sections
// also have the number of lines (taken from bodyLines)
//
// Multipart parts are the list of subparts followed by the subtype (from contentTypeLine)
// Note that even though I'm passing a parameter about whether or not I want to include the 
// extension data, I'm not actually putting extension data in the response
//
// Message parts are formatted just like text parts except they have the envelope structure
// and the body structure before the number of lines.
static std::string FetchResponseBodyStructureHelper(const MESSAGE_BODY &body, bool includeExtensionData)
{
    std::ostringstream result;
    result << "(";
    switch(body.bodyMediaType) {
    case MIME_TYPE_MULTIPART:
	if (NULL != body.subparts) {
	    for (int i=0; i<body.subparts->size(); ++i) {
		MESSAGE_BODY part = (*body.subparts)[i];
		result << FetchResponseBodyStructureHelper(part, includeExtensionData);
	    }
	}
	result << " " << ParseBodySubtype(body.contentTypeLine, body.bodyMediaType).c_str();
	break;

    case MIME_TYPE_MESSAGE:
	result << ParseBodyType(body.contentTypeLine).c_str();
	result << " " << ParseBodySubtype(body.contentTypeLine, body.bodyMediaType).c_str();
	{
	    insensitiveString uncommented = RemoveRfc822Comments(body.contentTypeLine);
	    int pos = uncommented.find(';');
	    if (std::string::npos == pos) {
		result << " NIL";
	    }
	    else {
		result << " " << ParseBodyParameters(body.contentTypeLine, body.bodyMediaType).c_str();
	    }
	}
	if (0 == body.contentIdLine.size()) {
	    result << " NIL";
	}
	else {
	    result << " \"" << body.contentIdLine.c_str() << "\"";
	}
	if (0 == body.contentDescriptionLine.size()) {
	    result << " NIL";
	}
	else {
	    result << " \"" << body.contentDescriptionLine.c_str() << "\"";
	}
	if (0 == body.contentEncodingLine.size()) {
	    result << " \"7BIT\"";
	}
	else {
	    result << " \"" << body.contentEncodingLine.c_str() << "\"";
	}
	if ((NULL != body.subparts) && (0<body.subparts->size())) {
	    result << " " << (body.bodyOctets - body.headerOctets);
	    MESSAGE_BODY part = (*body.subparts)[0];
	    result << " " << FetchSubpartEnvelope(part).c_str();
	    result << " " << FetchResponseBodyStructureHelper(part, includeExtensionData);
	}
	break;

    default:
	result << ParseBodyType(body.contentTypeLine).c_str();
	result << " " << ParseBodySubtype(body.contentTypeLine, body.bodyMediaType).c_str();
	result << " " << ParseBodyParameters(body.contentTypeLine, body.bodyMediaType).c_str();
	if (0 == body.contentIdLine.size()) {
	    result << " NIL";
	}
	else {
	    result << " \"" << body.contentIdLine.c_str() << "\"";
	}
	if (0 == body.contentDescriptionLine.size()) {
	    result << " NIL";
	}
	else {
	    result << " \"" << body.contentDescriptionLine.c_str() << "\"";
	}
	if (0 == body.contentEncodingLine.size()) {
	    result << " \"7BIT\"";
	}
	else {
	    result << " \"" << body.contentEncodingLine.c_str() << "\"";
	}
	result << " " << (body.bodyOctets - body.headerOctets);
	break;
    }
    if ((MIME_TYPE_TEXT == body.bodyMediaType) || (MIME_TYPE_MESSAGE == body.bodyMediaType)) {
	result << " " << (body.bodyLines - body.headerLines);
    }
    result << ")";
    return result.str();
}

void ImapSession::FetchResponseBodyStructure(const MailMessage *message)
{
    std::string result("BODYSTRUCTURE ");
    result +=  FetchResponseBodyStructureHelper(message->GetMessageBody(), true);
    m_s->Send((uint8_t *)result.c_str(), result.size());
}

void ImapSession::FetchResponseBody(const MailMessage *message)
{
    std::string result("BODY ");
    result +=  FetchResponseBodyStructureHelper(message->GetMessageBody(), false);
    m_s->Send((uint8_t *)result.c_str(), result.size());
}

void ImapSession::FetchResponseUid(unsigned long uid) {
    std::ostringstream result;
    result << "UID " << uid;
    m_s->Send((uint8_t *)result.str().c_str(), result.str().size());
}

#define SendBlank() (blankLen?m_s->Send((uint8_t *)" ",1):0)

IMAP_RESULTS ImapSession::FetchHandlerExecute(bool usingUid) {
    IMAP_RESULTS finalResult = IMAP_OK;

    size_t executePointer = strlen((char *)m_parseBuffer) + 1;
    executePointer += strlen((char *)&m_parseBuffer[executePointer]) + 1;
    SEARCH_RESULT srVector;

    if (usingUid) {
	executePointer += strlen((char *)&m_parseBuffer[executePointer]) + 1;
	UidSequenceSet(srVector, executePointer);
    }
    else {
	if (!MsnSequenceSet(srVector, executePointer)) {
	    strncpy(m_responseText, "Invalid Message Sequence Number", MAX_RESPONSE_STRING_LENGTH);
	    finalResult = IMAP_NO;
	}
    }

    for (int i = 0; (IMAP_BAD != finalResult) && (i < srVector.size()); ++i) {
	int blankLen = 0;
	MailMessage *message;

	MailMessage::MAIL_MESSAGE_RESULT messageReadResult = m_store->GetMessageData(&message, srVector[i]);
	unsigned long uid = srVector[i];
	if (MailMessage::SUCCESS == messageReadResult) {
	    IMAP_RESULTS result = IMAP_OK;

	    bool seenFlag = false;
	    size_t specificationBase;
// v-- This part syntax-checks the fetch request
	    specificationBase = executePointer + strlen((char *)&m_parseBuffer[executePointer]) + 1;
	    if (0 == strcmp("(", (char *)&m_parseBuffer[specificationBase])) {
		specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
	    }
	    while ((IMAP_OK == result) && (specificationBase < m_parsePointer)) {
		FETCH_NAME_T::iterator which = fetchSymbolTable.find((char *)&m_parseBuffer[specificationBase]);
		if (fetchSymbolTable.end() == which) {
		    // This may be an extended BODY name, so I have to check for that.
		    char *temp;
		    temp = strchr((char *)&m_parseBuffer[specificationBase], '[');
		    if (NULL != temp) {
			*temp = '\0';
			FETCH_NAME_T::iterator which = fetchSymbolTable.find((char *)&m_parseBuffer[specificationBase]);
			specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
			*temp = '[';
			if (fetchSymbolTable.end() != which) {
			    switch(which->second) {
			    case FETCH_BODY:
			    case FETCH_BODY_PEEK:
				break;

			    default:
				strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
				break;
			    }
			}
			else
			{
			    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			}
		    }
		    else
		    {
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }
		    MESSAGE_BODY body = message->GetMessageBody();
		    while ((IMAP_OK == result) && isdigit(m_parseBuffer[specificationBase])) {
			char *end;
			unsigned long section = strtoul((char *)&m_parseBuffer[specificationBase], &end, 10);
			// This part here is because the message types have two headers, one for the
			// subpart the message is in, one for the message that's contained in the subpart
			// if I'm looking at a subpart of that subpart, then I have to skip the subparts
			// header and look at the message's header, which is handled by the next four
			// lines of code.
			if ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size())) {
			    body = (*body.subparts)[0];
			}
			if (NULL != body.subparts) {
			    if ((0 < section) && (section <= (body.subparts->size()))) {
				body = (*body.subparts)[section-1];
				specificationBase += (end - ((char *)&m_parseBuffer[specificationBase]));
				if ('.' == *end) {
				    ++specificationBase;
				}
			    }
			    else {
				strncpy(m_responseText, "Invalid Subpart", MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
			else {
			    if ((1 == section) && (']' == *end)) {
				specificationBase += (end - ((char *)&m_parseBuffer[specificationBase]));
			    }
			    else {
				strncpy(m_responseText, "Invalid Subpart", MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
		    }
		    // When I get here, and if everything's okay, then I'm looking for a possibly nonzero length of characters
		    // representing the section-msgtext
		    FETCH_BODY_PARTS whichPart;
		    if ((IMAP_OK == result) && (m_parsePointer > specificationBase)) {
			// If I'm out of string and the next string begins with ']', then I'm returning the whole body of 
			// whatever subpart I'm at
			if (']' == m_parseBuffer[specificationBase]) {
			    whichPart = FETCH_BODY_BODY;
			}
			else {
			    insensitiveString part((char *)&m_parseBuffer[specificationBase]);
			    // I'm looking for HEADER[.FIELDS[.NOT]], TEXT, or MIME
			    if (part.substr(0, 6) == "header") {
				if (']' == part[6]) {
				    specificationBase += 6;
				    whichPart = FETCH_BODY_HEADER;
				}
				else {
				    if (part.substr(6, 7) == ".FIELDS") {
					if (13 == part.size()) {
					    specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
					    whichPart = FETCH_BODY_FIELDS;
					}
					else {
					    if (part.substr(13) == ".NOT") {
						specificationBase +=  strlen((char *)&m_parseBuffer[specificationBase]) + 1;
						whichPart = FETCH_BODY_NOT_FIELDS;
					    }
					    else {
						strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
						result = IMAP_BAD;
					    }
					}
				    }
				    else {
					strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
					result = IMAP_BAD;
				    }
				}
			    }
			    else {
				if (part.substr(0, 5) == "TEXT]") {
				    specificationBase += 4;
				    whichPart = FETCH_BODY_TEXT;
				}
				else {
				    if (part.substr(0, 5) == "MIME]") {
					specificationBase += 4;
					whichPart = FETCH_BODY_MIME;
				    }
				    else {
					strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
					result = IMAP_BAD;
				    }
				}
			    }
			}
		    }
		    // Now look for the list of headers to return
		    std::vector<std::string> fieldList;
		    if ((IMAP_OK == result) && (m_parsePointer > specificationBase) && 
 			((FETCH_BODY_FIELDS == whichPart) || (FETCH_BODY_NOT_FIELDS == whichPart))) {
			if (0 == strcmp((char *)&m_parseBuffer[specificationBase], "(")) {
			    specificationBase +=  strlen((char *)&m_parseBuffer[specificationBase]) + 1;
			    while((IMAP_BUFFER_LEN > specificationBase) &&
				  (0 != strcmp((char *)&m_parseBuffer[specificationBase], ")"))) {
				specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
			    }
			    if (0 == strcmp((char *)&m_parseBuffer[specificationBase], ")")) {
				specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
			    }
			    else {
				strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
			else {
			    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			}
		    }

		    // Okay, look for the end square bracket
		    if ((IMAP_OK == result) && (']' == m_parseBuffer[specificationBase])) {
			specificationBase++;
		    }
		    else {
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }

		    // Okay, get the limits, if any.
		    size_t firstByte = 0, maxLength = ~0;
		    if ((IMAP_OK == result) && (m_parsePointer > specificationBase)) {
			if ('<' == m_parseBuffer[specificationBase])
			{
			    ++specificationBase;
			    char *end;
			    firstByte = strtoul((char *)&m_parseBuffer[specificationBase], &end, 10);
			    if ('.' == *end) {
				maxLength = strtoul(end+1, &end, 10);
			    }
			    if ('>' != *end) {
				strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
		    }

		    if (IMAP_OK == result) {
			switch(whichPart) {
			case FETCH_BODY_BODY:
			case FETCH_BODY_MIME:
			case FETCH_BODY_HEADER:
			case FETCH_BODY_FIELDS:
			case FETCH_BODY_NOT_FIELDS:
			case FETCH_BODY_TEXT:
			    break;

			default:
			    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			    break;
			}

		    }
		}
		specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
		// SYZYGY -- Valgrind sometimes flags this as uninitialized
		// SYZYGY -- I suppose it's running off the end of m_parseBuffer
		if (0 == strcmp(")", (char *)&m_parseBuffer[specificationBase])) {
		    specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
		}
	    }
// ^-- This part syntax-checks the fetch request
// v-- This part executes the fetch request
	    if (IMAP_OK == result) {
		specificationBase = executePointer + strlen((char *)&m_parseBuffer[executePointer]) + 1;
		if (0 == strcmp("(", (char *)&m_parseBuffer[specificationBase])) {
		    specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
		}
		std::ostringstream fetchResult;
		fetchResult << "* " << message->GetMsn() << " FETCH (";
		m_s->Send((uint8_t *)fetchResult.str().c_str(), fetchResult.str().size());
	    }
	    while ((IMAP_OK == result) && (specificationBase < m_parsePointer)) {
		SendBlank();
		FETCH_NAME_T::iterator which = fetchSymbolTable.find((char *)&m_parseBuffer[specificationBase]);
		if (fetchSymbolTable.end() != which) {
		    blankLen = 1;
		    switch(which->second) {
		    case FETCH_ALL:
			FetchResponseFlags(message->GetMessageFlags());
			SendBlank();
			FetchResponseInternalDate(message);
			SendBlank();
			FetchResponseRfc822Size(message);
			SendBlank();
			FetchResponseEnvelope(message);
			break;

		    case FETCH_FAST:
			FetchResponseFlags(message->GetMessageFlags());
			SendBlank();
			FetchResponseInternalDate(message);
			SendBlank();
			FetchResponseRfc822Size(message);
			break;

		    case FETCH_FULL:
			FetchResponseFlags(message->GetMessageFlags());
			SendBlank();
			FetchResponseInternalDate(message);
			SendBlank();
			FetchResponseRfc822Size(message);
			SendBlank();
			FetchResponseEnvelope(message);
			SendBlank();
			FetchResponseBody(message);
			break;

		    case FETCH_BODY_PEEK:
		    case FETCH_BODY:
			FetchResponseBody(message);
			break;

		    case FETCH_BODYSTRUCTURE:
			FetchResponseBodyStructure(message);
			break;

		    case FETCH_ENVELOPE:
			FetchResponseEnvelope(message);
			break;

		    case FETCH_FLAGS:
			FetchResponseFlags(message->GetMessageFlags());
			break;

		    case FETCH_INTERNALDATE:
			FetchResponseInternalDate(message);
			break;

		    case FETCH_RFC822:
			FetchResponseRfc822(uid, message);
			seenFlag = true;
			break;
						
		    case FETCH_RFC822_HEADER:
			FetchResponseRfc822Header(uid, message);
			break;

		    case FETCH_RFC822_SIZE:
			FetchResponseRfc822Size(message);
			break;

		    case FETCH_RFC822_TEXT:
			FetchResponseRfc822Text(uid, message);
			seenFlag = true;
			break;

		    case FETCH_UID:
			FetchResponseUid(uid);
			break;

		    default:
			strncpy(m_responseText, "Internal Logic Error", MAX_RESPONSE_STRING_LENGTH);
//    					result = IMAP_NO;
			break;
		    }
		}
		else {
		    // This may be an extended BODY name, so I have to check for that.
		    char *temp;
		    temp = strchr((char *)&m_parseBuffer[specificationBase], '[');
		    if (NULL != temp) {
			*temp = '\0';
			FETCH_NAME_T::iterator which = fetchSymbolTable.find((char *)&m_parseBuffer[specificationBase]);
			specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
			*temp = '[';
			if (fetchSymbolTable.end() != which) {
			    switch(which->second) {
			    case FETCH_BODY:
				seenFlag = true;
				// NOTE NO BREAK!
			    case FETCH_BODY_PEEK:
				m_s->Send((uint8_t *) "BODY[", 5);
				break;

			    default:
				strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
				break;
			    }
			}
			else {
			    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			}
		    }
		    else {
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }
		    MESSAGE_BODY body = message->GetMessageBody();
		    // I need a part number flag because, for a single part message, body[1] is
		    // different from body[], but later on, when I determine what part to fetch,
		    // I won't know whether I've got body[1] or body[], and partNumberFlag
		    // records the difference
		    bool partNumberFlag = false;
		    while ((IMAP_OK == result) && isdigit(m_parseBuffer[specificationBase])) {
			partNumberFlag = true;
			char *end;
			unsigned long section = strtoul((char *)&m_parseBuffer[specificationBase], &end, 10);
			// This part here is because the message types have two headers, one for the
			// subpart the message is in, one for the message that's contained in the subpart
			// if I'm looking at a subpart of that subpart, then I have to skip the subparts
			// header and look at the message's header, which is handled by the next four
			// lines of code.
			if ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size())) {
			    body = (*body.subparts)[0];
			}
			if (NULL != body.subparts) {
			    if ((0 < section) && (section <= (body.subparts->size()))) {
				std::ostringstream sectionString;
				sectionString << section;
				m_s->Send((uint8_t *)sectionString.str().c_str(), sectionString.str().size());
				body = (*body.subparts)[section-1];
				specificationBase += (end - ((char *)&m_parseBuffer[specificationBase]));
				if ('.' == *end) {
				    m_s->Send((uint8_t *)".", 1);
				    ++specificationBase;
				}
			    }
			    else {
				strncpy(m_responseText, "Invalid Subpart", MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_NO;
			    }
			}
			else {
			    std::ostringstream sectionString;
			    sectionString << section;
			    m_s->Send((uint8_t *)sectionString.str().c_str(), sectionString.str().size());
			    specificationBase += (end - ((char *)&m_parseBuffer[specificationBase]));
			}
		    }
		    // When I get here, and if everything's okay, then I'm looking for a possibly nonzero length of characters
		    // representing the section-msgtext
		    FETCH_BODY_PARTS whichPart;
		    if ((IMAP_OK == result) && (m_parsePointer > specificationBase)) {
			// If I'm out of string and the next string begins with ']', then I'm returning the whole body of 
			// whatever subpart I'm at
			if (']' == m_parseBuffer[specificationBase]) {
			    // Unless it's a single part message -- in which case I return the text
			    if (!partNumberFlag || (NULL != body.subparts)) {
				whichPart = FETCH_BODY_BODY;
			    }
			    else {
				whichPart = FETCH_BODY_TEXT;
			    }
			}
			else {
			    insensitiveString part((char *)&m_parseBuffer[specificationBase]);
			    // I'm looking for HEADER[.FIELDS[.NOT]], TEXT, or MIME
			    if (part.substr(0, 6) == "header") {
				if (']' == part[6]) {
				    specificationBase += 6;
				    m_s->Send((uint8_t *) "HEADER", 6);
				    whichPart = FETCH_BODY_HEADER;
				}
				else {
				    if (part.substr(6,7) == ".FIELDS") {
					if (13 == part.size()) {
					    specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
					    m_s->Send((uint8_t *) "HEADER.FIELDS", 13);
					    whichPart = FETCH_BODY_FIELDS;
					}
					else {
					    if (part.substr(13) == ".NOT") {
						specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
						m_s->Send((uint8_t *) "HEADER.FIELDS.NOT", 17);
						whichPart = FETCH_BODY_NOT_FIELDS;
					    }
					    else {
						strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
						result = IMAP_BAD;
					    }
					}
				    }
				    else {
					strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
					result = IMAP_BAD;
				    }
				}
			    }
			    else {
				if (part.substr(0, 5) == "TEXT]") {
				    specificationBase += 4;
				    m_s->Send((uint8_t *) "TEXT", 4);
				    whichPart = FETCH_BODY_TEXT;
				}
				else {
				    if (part.substr(0, 5) == "MIME]") {
					specificationBase += 4;
					m_s->Send((uint8_t *) "MIME", 4);
					whichPart = FETCH_BODY_MIME;
				    }
				    else {
					strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
					result = IMAP_BAD;
				    }
				}
			    }
			}
		    }
		    // Now look for the list of headers to return
		    std::vector<insensitiveString> fieldList;
		    if ((IMAP_OK == result) && (m_parsePointer > specificationBase) &&
			((FETCH_BODY_FIELDS == whichPart) || (FETCH_BODY_NOT_FIELDS == whichPart))) {
			if (0 == strcmp((char *)&m_parseBuffer[specificationBase], "(")) {
			    m_s->Send((uint8_t *) " (", 2);
			    specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
			    int blankLen = 0;
			    while((IMAP_BUFFER_LEN > specificationBase) &&
				  (0 != strcmp((char *)&m_parseBuffer[specificationBase], ")"))) {
				int len = (int) strlen((char *)&m_parseBuffer[specificationBase]);
				SendBlank();
				m_s->Send((uint8_t *)&m_parseBuffer[specificationBase], len);
				blankLen = 1;
				fieldList.push_back((char*)&m_parseBuffer[specificationBase]);
				specificationBase += len + 1;
			    }
			    if (0 == strcmp((char *)&m_parseBuffer[specificationBase], ")")) {
				m_s->Send((uint8_t *) ")", 1);
				specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
			    }
			    else {
				strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
			else {
			    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			}
		    }

		    // Okay, look for the end square bracket
		    if ((IMAP_OK == result) && (']' == m_parseBuffer[specificationBase])) {
			m_s->Send((uint8_t *) "]", 1);
			specificationBase++;
		    }
		    else {
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }

		    // Okay, get the limits, if any.
		    size_t firstByte = 0, maxLength = ~0;
		    if ((IMAP_OK == result) && (m_parsePointer > specificationBase)) {
			if ('<' == m_parseBuffer[specificationBase]) {
			    ++specificationBase;
			    char *end;
			    firstByte = strtoul((char *)&m_parseBuffer[specificationBase], &end, 10);
			    std::ostringstream limit;
                            limit << "<" << firstByte << ">"; 
			    m_s->Send((uint8_t *) limit.str().c_str(), limit.str().size());
			    if ('.' == *end) {
				maxLength = strtoul(end+1, &end, 10);
			    }
			    if ('>' != *end) {
				strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
		    }

		    if (IMAP_OK == result) {
			size_t length;

			switch(whichPart) {
			case FETCH_BODY_BODY:
			    // This returns all the data header and text, but it's limited by first_byte and max_length
			    if ((body.bodyMediaType == MIME_TYPE_MESSAGE) || (partNumberFlag && (body.bodyMediaType == MIME_TYPE_MULTIPART))) {
				// message parts are special, I have to skip over the mime header before I begin
				firstByte += body.headerOctets;
			    }
			    if (firstByte < body.bodyOctets) {
				length = MIN(body.bodyOctets - firstByte, maxLength);
				m_s->Send((uint8_t *) " ", 1);
				SendMessageChunk(uid, firstByte + body.bodyStartOffset, length); 
			    }
			    else {
				m_s->Send((uint8_t *) " {0}\r\n", 6);
			    }
			    break;

			case FETCH_BODY_HEADER:
			    if (!partNumberFlag || ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size()))) {
				if (partNumberFlag) {
				    // It's a subpart
				    body = (*body.subparts)[0];
				}
				if (firstByte < body.headerOctets)
				{
				    length = MIN(body.headerOctets - firstByte, maxLength);
				    m_s->Send((uint8_t *) " ", 1);
				    SendMessageChunk(uid, firstByte + body.bodyStartOffset, length);
				}
				else {
				    m_s->Send((uint8_t *) " {0}\r\n", 6);
				}
			    }
			    else {
				// If it's not a message subpart, it doesn't have a header.
				m_s->Send((uint8_t *) " \"\"", 3);
			    }
			    break;

			case FETCH_BODY_MIME:
			    if (firstByte < body.headerOctets)
			    {
				length = MIN(body.headerOctets - firstByte, maxLength);
				m_s->Send((uint8_t *) " ", 1);
				SendMessageChunk(uid, firstByte + body.bodyStartOffset, length);
			    }
			    else {
				m_s->Send((uint8_t *) " {0}\r\n", 6);
			    }
			    break;

			case FETCH_BODY_FIELDS:
			    if ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size())) {
				body = (*body.subparts)[0];
			    }
			    {
				std::string interesting;

				// For each element in fieldList, look up the corresponding entries in body.fieldList
				// and display
				for(int i=0; i<fieldList.size(); ++i) {
				    for (HEADER_FIELDS::const_iterator iter = body.fieldList.lower_bound(fieldList[i]);
					 body.fieldList.upper_bound(fieldList[i]) != iter; ++iter) {
					interesting += iter->first.c_str();
					interesting += ": ";
					interesting += iter->second.c_str();
					interesting += "\r\n";
				    }
				}
				interesting += "\r\n";
				if (firstByte < interesting.size())
				{
				    length = MIN(interesting.size() - firstByte, maxLength);

				    std::ostringstream headerFields;

				    headerFields << " {" << length << "}\r\n";
				    headerFields << interesting.substr(firstByte, length);
				    m_s->Send((uint8_t *)headerFields.str().c_str(), headerFields.str().size());
				}
				else {
				    m_s->Send((uint8_t *) " {0}\r\n", 6);
				}
			    }
			    break;

			case FETCH_BODY_NOT_FIELDS:
			    if ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size())) {
				body = (*body.subparts)[0];
			    }
			    {
				insensitiveString interesting;

				for (HEADER_FIELDS::const_iterator i = body.fieldList.begin(); body.fieldList.end() != i; ++i) {
				    if (fieldList.end() == std::find(fieldList.begin(), fieldList.end(), i->first)) {
					interesting += i->first;
					interesting += ": ";
					interesting += i->second;
					interesting += "\r\n";
				    }
				}
				interesting += "\r\n";
				if (firstByte < interesting.size()) {
				    length = MIN(interesting.size() - firstByte, maxLength);

				    std::ostringstream headerFields;
				    headerFields << " {" << length << "}\r\n";
				    headerFields << interesting.substr(firstByte, length).c_str();
				    m_s->Send((uint8_t *)headerFields.str().c_str(), headerFields.str().size());
				}
				else {
				    m_s->Send((uint8_t *) " {0}\r\n", 6);
				}
			    }
			    break;

			case FETCH_BODY_TEXT:
			    if ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size())) {
				body = (*body.subparts)[0];
			    }
			    if (0 < body.headerOctets) {
				std::string result;
				size_t length;
				if (body.bodyOctets > body.headerOctets) {
				    length = body.bodyOctets - body.headerOctets;
				}
				else {
				    length = 0;
				}
				if (firstByte < length) {
				    length = MIN(length - firstByte, maxLength);
				    m_s->Send((uint8_t *) " ", 1);
				    SendMessageChunk(uid, firstByte + body.bodyStartOffset + body.headerOctets, length);
				}
				else {
				    m_s->Send((uint8_t *) " {0}\r\n", 6);
				}
			    }
			    else {
				m_s->Send((uint8_t *) " {0}\r\n", 6);
			    }
			    break;

			default:
			    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			    break; 
			}
		    }
		}
		blankLen = 1;
		specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
		if (0 == strcmp(")", (char *)&m_parseBuffer[specificationBase])) {
		    specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
		}
	    }
	    // Here, I update the SEEN flag, if that is necessary, and send the flags if that flag has
	    // changed state
	    if (IMAP_OK == result) {
		if (seenFlag && (0 == (MailStore::IMAP_MESSAGE_SEEN & message->GetMessageFlags()))) {
		    uint32_t updatedFlags;
		    if (MailStore::SUCCESS == m_store->MessageUpdateFlags(srVector[i], ~0, MailStore::IMAP_MESSAGE_SEEN, updatedFlags)) {
			SendBlank();
			FetchResponseFlags(updatedFlags);
			blankLen = 1;
		    }
		}
		if (usingUid) {
		    SendBlank();
		    FetchResponseUid(uid);
		    blankLen = 1;
		}
		m_s->Send((uint8_t *) ")\r\n", 3);
		blankLen = 0;
	    }
	    else {
		 finalResult = result;
	    }
	}
	else {
	    finalResult = IMAP_NO;
	    if (MailMessage::MESSAGE_DOESNT_EXIST != messageReadResult) {
		strncpy(m_responseText, "Internal Mailstore Error", MAX_RESPONSE_STRING_LENGTH);
	    }
	    else {
		strncpy(m_responseText, "Message Doesn't Exist", MAX_RESPONSE_STRING_LENGTH);
	    }
	}
// ^-- This part executes the fetch request
    }
    return finalResult;
}

// The problem is that I'm using m_parseStage to do the parenthesis balance, so this needs to be redesigned
IMAP_RESULTS ImapSession::FetchHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool usingUid)
{
    IMAP_RESULTS result = IMAP_OK;

    m_usesUid = usingUid;
    // The first thing up is a sequence set.  This is a sequence of digits, commas, and colons, followed by
    // a space
    if (0 == m_parseStage) {
	size_t len = strspn(((char *)data)+parsingAt, "0123456789,:*");
	if ((0 < len) && (dataLen >= (parsingAt + len + 2)) && (' ' == data[parsingAt+len]) && 
	    (IMAP_BUFFER_LEN > (m_parsePointer+len+1)))  {
	    AddToParseBuffer(&data[parsingAt], len);
	    parsingAt += len + 1;

	    m_parseStage = 1;
	    if ('(' == data[parsingAt]) {
		AddToParseBuffer(&data[parsingAt], 1);
		++parsingAt;
		++m_parseStage;
		while ((IMAP_OK == result) && (dataLen > parsingAt) && (1 < m_parseStage)) {
		    if((dataLen > parsingAt) && (' ' == data[parsingAt])) {
			++parsingAt;
		    }
		    if((dataLen > parsingAt) && ('(' == data[parsingAt])) {
			AddToParseBuffer(&data[parsingAt], 1);
			++parsingAt;
			++m_parseStage;
		    }
		    switch (astring(data, dataLen, parsingAt, true, NULL)) {
		    case ImapStringGood:
			result = IMAP_OK;
			break;

		    case ImapStringBad:
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
			break;

		    case ImapStringPending:
			strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_NOTDONE;
			break;

		    default:
			strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
			break;
		    }
		    if ((dataLen > parsingAt) && (')' == data[parsingAt])) {
			AddToParseBuffer(&data[parsingAt], 1);
			++parsingAt;
			--m_parseStage;
		    }
		}
		if ((IMAP_OK == result) && (1 == m_parseStage)) {
		    if (dataLen == parsingAt) {
			result = FetchHandlerExecute(usingUid);
		    }
		    else {
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }
		}
	    }
	    else {
		while ((IMAP_OK == result) && (dataLen > parsingAt)) {
		    if (' ' == data[parsingAt]) {
			++parsingAt;
		    }
		    if ((dataLen > parsingAt) && ('(' == data[parsingAt])) {
			AddToParseBuffer(&data[parsingAt], 1);
			++parsingAt;
		    }
		    if ((dataLen > parsingAt) && (')' == data[parsingAt])) {
			AddToParseBuffer(&data[parsingAt], 1);
			++parsingAt;
		    }
		    switch (astring(data, dataLen, parsingAt, true, NULL)) {
		    case ImapStringGood:
			break;

		    case ImapStringBad:
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
			break;

		    case ImapStringPending:
			++m_parseStage;
			strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_NOTDONE;
			break;

		    default:
			strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
			break;
		    }
		}
		if (IMAP_OK == result) {
		    result = FetchHandlerExecute(usingUid);
		}
	    }
	}
	else {
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
    }
    else {
	if (dataLen >= m_literalLength) {
	    AddToParseBuffer(data, m_literalLength);
	    size_t i = m_literalLength;
	    m_literalLength = 0;
	    if ((i < dataLen) && (' ' == data[i])) {
		++i;
	    }
	    if (2 < dataLen) {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    if (1 == m_parseStage) {
		IMAP_RESULTS result = IMAP_OK;
		while ((IMAP_OK == result) && (dataLen > i)) {
		    if((dataLen > i) && (' ' == data[i])) {
			++i;
		    }
		    if((dataLen > i) && ('(' == data[i])) {
			AddToParseBuffer(&data[i], 1);
			++i;
		    }
		    if ((dataLen > i) && (')' != data[i])) {
			switch (astring(data, dataLen, i, true, NULL)) {
			case ImapStringGood:
			    result = IMAP_OK;
			    break;

			case ImapStringBad:
			    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			    break;

			case ImapStringPending:
			    strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_NOTDONE;
			    break;

			default:
			    strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			    break;
			}
		    }
		    if ((dataLen > i) && (')' == data[i])) {
			AddToParseBuffer(&data[i], 1);
			++i;
		    }
		}
		if (IMAP_OK == result) {
		    result = FetchHandlerExecute(m_usesUid);
		}
	    }
	    else {
		IMAP_RESULTS result = IMAP_OK;
		while ((IMAP_OK == result) && (dataLen > i) && (')' != data[i]) && (1 < m_parseStage)) {
		    if((dataLen > i) && (' ' == data[i])) {
			++i;
		    }
		    if((dataLen > i) && ('(' == data[i])) {
			AddToParseBuffer(&data[i], 1);
			++i;
			++m_parseStage;
		    }
		    switch (astring(data, dataLen, i, true, NULL)) {
		    case ImapStringGood:
			result = IMAP_OK;
			break;

		    case ImapStringBad:
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
			break;

		    case ImapStringPending:
			strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_NOTDONE;
			break;

		    default:
			strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
			break;
		    }
		    if ((dataLen > i) && (')' == data[i])) {
			AddToParseBuffer(&data[i], 1);
			++i;
			--m_parseStage;
		    }
		}
		if ((IMAP_OK == result) && (1 == m_parseStage)) {
		    if (dataLen == i) {
			result = FetchHandlerExecute(m_usesUid);
		    }
		    else {
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }
		}
		if (IMAP_NOTDONE != result) {
		    result = FetchHandlerExecute(m_usesUid);
		}
	    }
	}
	else {
	    result = IMAP_IN_LITERAL;
	    AddToParseBuffer(data, dataLen, false);
	    m_literalLength -= dataLen;
	}
    }
    return result;
}

IMAP_RESULTS ImapSession::StoreHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool usingUid) {
    IMAP_RESULTS result = IMAP_OK;
    size_t executePointer = m_parsePointer;
    int update = '=';

    while((parsingAt < dataLen) && (' ' != data[parsingAt])) {
	AddToParseBuffer(&data[parsingAt++], 1, false);
    }
    AddToParseBuffer(NULL, 0);
    if ((parsingAt < dataLen) && (' ' == data[parsingAt])) {
	++parsingAt;
    }
    else {
	strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
    }

    if ((IMAP_OK == result) && (('-' == data[parsingAt]) || ('+' == data[parsingAt]))) {
	update = data[parsingAt++];
    }
    while((IMAP_OK == result) && (parsingAt < dataLen) && (' ' != data[parsingAt])) {
	uint8_t temp = toupper(data[parsingAt++]);
	AddToParseBuffer(&temp, 1, false);
    }
    AddToParseBuffer(NULL, 0);
    if ((parsingAt < dataLen) && (' ' == data[parsingAt])) {
	++parsingAt;
    }
    else {
	strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
    }

    size_t myDataLen = dataLen;
    if ((IMAP_OK == result) && (parsingAt < dataLen) && ('(' == data[parsingAt])) {
	++parsingAt;
	if (')' == data[dataLen-1]) {
	    --myDataLen;
	}
	else {
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
    }

    while((IMAP_OK == result) && (parsingAt < myDataLen)) {
	// These can only be spaces, backslashes, or alpha
	if (isalpha(data[parsingAt]) || (' ' == data[parsingAt]) || ('\\' == data[parsingAt])) {
	    if (' ' == data[parsingAt]) {
		++parsingAt;
		AddToParseBuffer(NULL, 0);
	    }
	    else {
		uint8_t temp = toupper(data[parsingAt++]);
		AddToParseBuffer(&temp, 1, false);
	    }
	}
	else {
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
    }
    AddToParseBuffer(NULL, 0);
    if (parsingAt != myDataLen) {
	strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
    }

    if (IMAP_OK == result) {
	SEARCH_RESULT srVector;
	bool sequenceOk;
	if (usingUid) {
	    sequenceOk = UidSequenceSet(srVector, executePointer);
	}
	else {
	    sequenceOk = MsnSequenceSet(srVector, executePointer);
	}
	if (sequenceOk) {
	    executePointer += strlen((char *)&m_parseBuffer[executePointer]) + 1;
	    if (0 == strncmp((char *)&m_parseBuffer[executePointer], "FLAGS", 5)) {
		bool silentFlag = false;
		// I'm looking for "FLAGS.SILENT and the "+5" is because FLAGS is five characters long
		if ('.' == m_parseBuffer[executePointer+5]) {
		    if (0 == strcmp((char *)&m_parseBuffer[executePointer+6], "SILENT")) {
			silentFlag = true;
		    }
		    else {
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }
		}
		if (IMAP_OK == result) {
		    uint32_t flagSet = 0;

		    executePointer += strlen((char *)&m_parseBuffer[executePointer]) + 1;
		    while((IMAP_OK == result) && (m_parsePointer > executePointer)) {
			if ('\\' == m_parseBuffer[executePointer]) {
			    FLAG_SYMBOL_T::iterator found = flagSymbolTable.find((char *)&m_parseBuffer[executePointer+1]);
			    if (flagSymbolTable.end() != found) {
				flagSet |= found->second;
			    }
			    else {
				strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
			else {
			    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			}
			executePointer += strlen((char *)&m_parseBuffer[executePointer]) + 1;
		    }
		    if (IMAP_OK == result) {
			uint32_t andMask, orMask;
			switch (update) {
			case '+':
			    // Add these flags
			    andMask = ~0;
			    orMask = flagSet;
			    break;
	
			case '-':
			    // Remove these flags
			    andMask = ~flagSet;
			    orMask = 0;
			    break;
	
			default:
			    // Set these flags
			    andMask = 0;
			    orMask = flagSet;
			    break;
			}
			for (int i=0; i < srVector.size(); ++i) {
			    if (0 != srVector[i]) {
				uint32_t flags;
				if (MailStore::SUCCESS == m_store->MessageUpdateFlags(srVector[i], andMask, orMask, flags)) {
				    if (!silentFlag) {
					std::ostringstream fetch;
					fetch << "* " << m_store->MailboxUidToMsn(srVector[i]) << " FETCH (";
					m_s->Send((uint8_t *)fetch.str().c_str(), fetch.str().size());
					FetchResponseFlags(flags);
					if (usingUid) {
					    m_s->Send((uint8_t *)" ", 1);
					    FetchResponseUid(srVector[i]);
					}
					m_s->Send((uint8_t *)")\r\n", 3);
				    }
				}
				else {
				    result = IMAP_MBOX_ERROR;
				}
			    }
			}
		    }
		}
	    }
	    else {
		strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		result = IMAP_BAD;
	    }
	}
	else {
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
    }
    return result;
}

 
IMAP_RESULTS ImapSession::CopyHandlerExecute(bool usingUid) {
    IMAP_RESULTS result = IMAP_MBOX_ERROR;
    size_t execute_pointer = 0;

    // Skip the first two tokens that are the tag and the command
    execute_pointer += strlen((char *)m_parseBuffer) + 1;
    execute_pointer += strlen((char *)&m_parseBuffer[execute_pointer]) + 1;

    // I do this once again if bUsingUid is set because the command in that case is UID
    if (usingUid) {
	execute_pointer += strlen((char *)&m_parseBuffer[execute_pointer]) + 1;
    }

    SEARCH_RESULT vector;
    bool sequenceOk;
    if (usingUid) {
	sequenceOk = UidSequenceSet(vector, execute_pointer);
    }
    else {
	sequenceOk = MsnSequenceSet(vector, execute_pointer);
    }
    if (sequenceOk) {
	NUMBER_LIST messageUidsAdded;

	execute_pointer += strlen((char *)&m_parseBuffer[execute_pointer]) + 1;
	std::string mailboxName((char *)&m_parseBuffer[execute_pointer]);
	for (SEARCH_RESULT::iterator i=vector.begin(); (i!=vector.end()) && (IMAP_OK==result); ++i) {
	    MailMessage *message;

	    MailMessage::MAIL_MESSAGE_RESULT messageReadResult = m_store->GetMessageData(&message, *i);
	    if (MailMessage::SUCCESS == messageReadResult) {
		size_t newUid;
		DateTime when = m_store->MessageInternalDate(*i);
		uint32_t flags = message->GetMessageFlags();

		char buffer[m_store->GetBufferLength(*i)+1];
		switch (m_mboxErrorCode = m_store->OpenMessageFile(*i)) {
		case MailStore::SUCCESS: {
		    size_t size = m_store->ReadMessage(buffer, 0, m_store->GetBufferLength(*i));
		    switch (m_mboxErrorCode = m_store->AddMessageToMailbox(mailboxName, (uint8_t *)buffer, size, when, flags, &newUid)) {
		    case MailStore::SUCCESS:
			m_store->DoneAppendingDataToMessage(mailboxName, newUid);
			messageUidsAdded.push_back(newUid);
			result = IMAP_OK;
			break;

		    case MailStore::CANNOT_COMPLETE_ACTION:
			result = IMAP_TRY_AGAIN;
			break;
		    }
		}
		    break;

		case MailStore::CANNOT_COMPLETE_ACTION:
		    result = IMAP_TRY_AGAIN;
		    break;
		}
	    }
	}
	if (IMAP_OK != result) {
	    for (NUMBER_LIST::iterator i=messageUidsAdded.begin(); i!=messageUidsAdded.end(); ++i) {
		m_store->DeleteMessage(mailboxName, *i);
	    }
	}
    }
    else {
	strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
    }

    return result;
}


IMAP_RESULTS ImapSession::CopyHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool usingUid) {
    IMAP_RESULTS result = IMAP_OK;

    if (0 == m_parseStage) {
	while((parsingAt < dataLen) && (' ' != data[parsingAt])) {
	    AddToParseBuffer(&data[parsingAt++], 1, false);
	}
	AddToParseBuffer(NULL, 0);
	if ((parsingAt < dataLen) && (' ' == data[parsingAt])) {
	    ++parsingAt;
	}
	else {
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}

	if (IMAP_OK == result) {
	    switch (astring(data, dataLen, parsingAt, false, NULL)) {
	    case ImapStringGood:
		result = CopyHandlerExecute(usingUid);
		break;

	    case ImapStringBad:
		strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		result = IMAP_BAD;
		break;

	    case ImapStringPending:
		result = IMAP_NOTDONE;
		m_usesUid = usingUid;
		strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
		break;

	    default:
		strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
		break;
	    }
	}
    }
    else {
	if (dataLen >= m_literalLength)
	{
	    ++m_parseStage;
	    AddToParseBuffer(data, m_literalLength);
	    size_t i = m_literalLength;
	    m_literalLength = 0;
	    if ((i < dataLen) && (' ' == data[i])) {
		++i;
	    }
	    if (2 < dataLen) {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    m_responseText[0] = '\0';
	    CopyHandlerExecute(m_usesUid);
	}
	else {
	    result = IMAP_IN_LITERAL;
	    AddToParseBuffer(data, dataLen, false);
	    m_literalLength -= dataLen;
	}
    }
    return result;
}


IMAP_RESULTS ImapSession::UidHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool flag) {
	IMAP_RESULTS result;

	m_commandString = m_parsePointer;
	m_currentHandler = NULL;
	atom(data, dataLen, parsingAt);
	if (' ' == data[parsingAt]) {
	    ++parsingAt;
	    UID_SUBCOMMAND_T::iterator i = uidSymbolTable.find((char *)&m_parseBuffer[m_commandString]);
	    if (uidSymbolTable.end() != i) {
		switch(i->second) {
		case UID_STORE:
		    m_currentHandler = &ImapSession::StoreHandler;
		    break;

		case UID_FETCH:
		    m_currentHandler = &ImapSession::FetchHandler;
		    break;

		case UID_SEARCH:
		    m_currentHandler = &ImapSession::SearchHandler;
		    break;

		case UID_COPY:
		    m_currentHandler = &ImapSession::CopyHandler;
		    break;

		default:
		    break;
		}
	    }
	    if (NULL != m_currentHandler) {
		result = (this->*m_currentHandler)(data, dataLen, parsingAt, m_flag);
	    }
	    else {
		result = UnimplementedHandler();
	    }
	}
	else {
		strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		result = IMAP_BAD;
	}
	return result;
}
