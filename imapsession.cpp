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

// I have a timer that can fire and handle several different cases:
//   Look for updated data for asynchronous notifies
//   ***DONE*** Delay for failed LOGIN's or AUTHENTICATIONS
//   ***DONE*** Handle idle timeouts, both for IDLE mode and inactivity in other modes.


// SYZYGY -- I should allow the setting of the keepalive socket option
// SYZYGY -- refactor the handling of options and do the /good/bad/continued stuff

#include <sstream>
#include <algorithm>

#include <time.h>
#include <sys/fsuid.h>
#include <string.h>
#include <stdlib.h>

#include "delayedmessage.hpp"
#include "idletimer.hpp"
#include "asynchronousaction.hpp"
#include "retry.hpp"
#include "imapsession.hpp"
#include "imapmaster.hpp"
#include "sasl.hpp"
#include "unimplementedhandler.hpp"
#include "capabilityhandler.hpp"
#include "noophandler.hpp"
#include "logouthandler.hpp"
#include "authenticatehandler.hpp"
#include "loginhandler.hpp"
#include "namespacehandler.hpp"
#include "createhandler.hpp"
#include "deletehandler.hpp"
#include "renamehandler.hpp"
#include "subscribehandler.hpp"
#include "listhandler.hpp"
#include "statushandler.hpp"
#include "appendhandler.hpp"
#include "selecthandler.hpp"
#include "checkhandler.hpp"
#include "expungehandler.hpp"
#include "copyhandler.hpp"
#include "closehandler.hpp"
#include "searchhandler.hpp"
#include "fetchhandler.hpp"
#include "storehandler.hpp"

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

// #include <iostream>
#include <list>

typedef enum {
    UID_STORE,
    UID_FETCH,
    UID_SEARCH,
    UID_COPY
} UID_SUBCOMMAND_VALUES;
typedef std::map<std::string, UID_SUBCOMMAND_VALUES> UID_SUBCOMMAND_T;

static UID_SUBCOMMAND_T uidSymbolTable;
bool ImapSession::m_anonymousEnabled = false;

IMAPSYMBOLS ImapSession::m_symbols;

const char *ParseBuffer::arguments() const {
    return (const char *) &m_parseBuffer[m_argumentPtr];
}

void ParseBuffer::argumentsHere(void) {
    m_argumentPtr = m_parsePointer;
}

const char *ParseBuffer::argument2() const {
    return (const char *) &m_parseBuffer[m_argument2Ptr];
}

void ParseBuffer::argument2Here(void) {
    m_argument2Ptr = m_parsePointer;
}

const char *ParseBuffer::argument3() const {
    return (const char *) &m_parseBuffer[m_argument3Ptr];
}

void ParseBuffer::argument3Here(void) {
    m_argument3Ptr = m_parsePointer;
}

void ParseBuffer::atom(INPUT_DATA_STRUCT &input) {
    INPUT_DATA_STRUCT t;
    t.parsingAt = 0;
    t.data = NULL;
    t.dataLen = 0;
    static char include_list[] = "!#$&'+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[^_`abcdefghijklmnopqrstuvwxyz|}~";
 
    while (input.parsingAt < input.dataLen) {
	if (NULL != strchr(include_list, input.data[input.parsingAt])) {
	    uint8_t temp = toupper(input.data[input.parsingAt]);
	    t.data = &temp;
	    t.dataLen = 1;
	    t.parsingAt = 0;
	    addToParseBuffer(t, 1, false);
	}
	else {
	    addToParseBuffer(t, 0);
	    break;
	}
	++input.parsingAt;
    }
}

enum ImapStringState ParseBuffer::astring(INPUT_DATA_STRUCT &input, bool makeUppercase, const char *additionalChars) {
    ImapStringState result = ImapStringGood; 
    INPUT_DATA_STRUCT t;
    t.parsingAt = 0;
    t.data = NULL;
    t.dataLen = 0;

    if (input.parsingAt < input.dataLen) {
	if ('"' == input.data[input.parsingAt]) {
	    input.parsingAt++; // Skip the opening quote
	    bool notdone = true, escape_flag = false;
	    do {
		switch(input.data[input.parsingAt]) {
		case '"':
		    if (escape_flag) {
			addToParseBuffer(input, 1, false);
			escape_flag = false;
		    }
		    else {
			addToParseBuffer(t, 0);
			notdone = false;
		    }
		    break;

		case '\\':
		    if (escape_flag) {
			addToParseBuffer(input, 1, false);
		    }
		    escape_flag = !escape_flag;
		    break;

		default:
		    escape_flag = false;
		    if (makeUppercase) {
			uint8_t temp = toupper(input.data[input.parsingAt]);
			t.data = &temp;
			t.dataLen = 1;
			t.parsingAt = 0;
			addToParseBuffer(t, 1, false);
			++input.parsingAt;
		    }
		    else {
			addToParseBuffer(input, 1, false);
		    }
		}
		// Check to see if I've run off the end of the input buffer
		if ((input.parsingAt > input.dataLen) || ((input.parsingAt == input.dataLen) && notdone)) {
		    notdone = false;
		    result = ImapStringBad;
		}
	    } while(notdone);
	}
	else {
	    if ('{' == input.data[input.parsingAt]) {
		// It's a literal string
		result = ImapStringPending;
		input.parsingAt++; // Skip the curly brace
		char *close;
		// read the number
		m_literalLength = strtoul((char *)&input.data[input.parsingAt], &close, 10);
		// check for the close curly brace and end of message and to see if I can fit all that data
		// into the buffer
		size_t lastChar = (size_t) (close - ((char *)input.data));
		if ((NULL == close) || ('}' != close[0]) || (lastChar != (input.dataLen - 1))) {
		    result = ImapStringBad;
		}
	    }
	    else {
		// It looks like an atom
		static char include_list[] = "!#$&'+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz|}~";
		result = ImapStringBad;

		while (input.parsingAt < input.dataLen) {
		    if ((NULL == strchr(include_list, input.data[input.parsingAt])) &&
			((NULL == additionalChars) || (NULL == strchr(additionalChars, input.data[input.parsingAt])))) {
			break;
		    }
		    result = ImapStringGood;
		    if (makeUppercase) {
			uint8_t temp = toupper(input.data[input.parsingAt]);
			t.data = &temp;
			t.dataLen = 1;
			t.parsingAt = 0;
			addToParseBuffer(t, 1, false);
			++input.parsingAt;
		    }
		    else {
			addToParseBuffer(input, 1, false);
		    }
		}
		addToParseBuffer(t, 0);
	    }
	}
    }
    return result;
}


ParseBuffer::ParseBuffer(size_t initial_size) {
    m_parseBuffer = new uint8_t[initial_size];
    m_parseBuffLen = initial_size;
    m_parsePointer = 0;
    m_literalLength = 0;
    m_argumentPtr = 0;
    m_commandPtr = 0;
}

ParseBuffer::~ParseBuffer() {
    delete[] m_parseBuffer;
}

void ParseBuffer::addToParseBuffer(INPUT_DATA_STRUCT &input, size_t length, bool nulTerminate) {
    // I need the +1 because I'm going to add a '\0' to the end of the string
    if ((m_parsePointer + length + 1) > m_parseBuffLen) {
	size_t newSize = MAX(m_parsePointer + length + 1, 2 * m_parseBuffLen);
	uint8_t *temp = new uint8_t[newSize];
	memcpy(temp, m_parseBuffer, m_parsePointer);
	delete[] m_parseBuffer;
	m_parseBuffLen = newSize;
	m_parseBuffer = temp;
    }
    if (0 < length) {
	memcpy(&m_parseBuffer[m_parsePointer], &input.data[input.parsingAt], length);
	input.parsingAt += length;
    }
    m_parsePointer += length;
    m_parseBuffer[m_parsePointer] = '\0';
    if (nulTerminate) {
	m_parsePointer++;
    }
}


// This returns the amount of data taken out of the buffer
uint32_t ParseBuffer::addLiteralToParseBuffer(INPUT_DATA_STRUCT &input) {
    uint32_t result;

    result = 0;
    if (input.dataLen >= m_literalLength) {
	addToParseBuffer(input, m_literalLength);
	result = m_literalLength;
	m_literalLength = 0;
    }
    else {
	addToParseBuffer(input, input.dataLen);
	result = input.dataLen;
	m_literalLength -= input.dataLen;
    }
    input.parsingAt = result;
    return result;
}

FLAG_SYMBOL_T flagSymbolTable;

void ParseBuffer::buildSymbolTable() {
    // These are only the standard flags.  User-defined flags must be handled differently
    flagSymbolTable.insert(FLAG_SYMBOL_T::value_type("ANSWERED", MailStore::IMAP_MESSAGE_ANSWERED));
    flagSymbolTable.insert(FLAG_SYMBOL_T::value_type("FLAGGED",  MailStore::IMAP_MESSAGE_FLAGGED));
    flagSymbolTable.insert(FLAG_SYMBOL_T::value_type("DELETED",  MailStore::IMAP_MESSAGE_DELETED));
    flagSymbolTable.insert(FLAG_SYMBOL_T::value_type("SEEN",     MailStore::IMAP_MESSAGE_SEEN));
    flagSymbolTable.insert(FLAG_SYMBOL_T::value_type("DRAFT",    MailStore::IMAP_MESSAGE_DRAFT));
}

size_t ParseBuffer::readEmailFlags(INPUT_DATA_STRUCT &input, bool &okay) {
    okay = true;
    uint32_t result = 0;

    // Read atoms until I run out of space or the next character is a ')' instead of a ' '
    while ((input.parsingAt <= input.dataLen) && (')' != input.data[input.parsingAt])) {
	uint32_t flagStart = m_parsePointer;
	if ('\\' == input.data[input.parsingAt++]) {
	    atom(input);
	    FLAG_SYMBOL_T::iterator i = flagSymbolTable.find((char *)&m_parseBuffer[flagStart]);
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
	if ((input.parsingAt < input.dataLen) && (' ' == input.data[input.parsingAt])) {
	    ++input.parsingAt;
	}
    }
    return result;
}


ImapHandler *uidHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
    ImapHandler *result;

    result = unimplementedHandler(session, input);
    session->parseBuffer()->commandHere();
    session->parseBuffer()->atom(input);
    if (' ' == input.data[input.parsingAt]) {
	++input.parsingAt;
	UID_SUBCOMMAND_T::iterator i = uidSymbolTable.find(session->parseBuffer()->command());
	if (uidSymbolTable.end() != i) {
	    switch(i->second) {
	    case UID_STORE:
		result = uidStoreHandler(session, input);
		break;
		
	    case UID_FETCH:
		result = uidFetchHandler(session, input);
		break;

	    case UID_SEARCH:
		result = uidSearchHandler(session, input);
		break;

	    case UID_COPY:
		result = uidCopyHandler(session, input);
		break;

	    default:
		break;
	    }
	}
    }
    return result;
}


void ImapSession::buildSymbolTables() {
    MailMessage::buildSymbolTable();

    symbol symbolToInsert;
    symbolToInsert.levels[0] = true;
    symbolToInsert.levels[1] = true;
    symbolToInsert.levels[2] = true;
    symbolToInsert.levels[3] = true;
    symbolToInsert.sendUpdatedStatus = true;
    symbolToInsert.handler = capabilityHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("CAPABILITY", symbolToInsert));
    symbolToInsert.handler = noopHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("NOOP", symbolToInsert));
    symbolToInsert.handler = logoutHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("LOGOUT", symbolToInsert));

    symbolToInsert.levels[0] = true;
    symbolToInsert.levels[1] = false;
    symbolToInsert.levels[2] = false;
    symbolToInsert.levels[3] = false;
    symbolToInsert.sendUpdatedStatus = true;
    symbolToInsert.handler = unimplementedHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("STARTTTLS", symbolToInsert));
    symbolToInsert.handler = authenticateHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("AUTHENTICATE", symbolToInsert));
    symbolToInsert.handler = loginHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("LOGIN", symbolToInsert));

    symbolToInsert.levels[0] = false;
    symbolToInsert.levels[1] = true;
    symbolToInsert.levels[2] = true;
    symbolToInsert.levels[3] = false;
    symbolToInsert.sendUpdatedStatus = true;
    symbolToInsert.handler = namespaceHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("NAMESPACE", symbolToInsert));
    symbolToInsert.handler = createHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("CREATE", symbolToInsert));
    symbolToInsert.handler = deleteHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("DELETE", symbolToInsert));
    symbolToInsert.handler = renameHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("RENAME", symbolToInsert));
    symbolToInsert.handler = unsubscribeHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("UNSUBSCRIBE", symbolToInsert));
    symbolToInsert.handler = subscribeHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("SUBSCRIBE", symbolToInsert));
    symbolToInsert.handler = listHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("LIST", symbolToInsert));
    symbolToInsert.handler = lsubHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("LSUB", symbolToInsert));
    symbolToInsert.handler = statusHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("STATUS", symbolToInsert));
    symbolToInsert.handler = appendHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("APPEND", symbolToInsert));
    // My justification for not allowing the updating of the status after the select and examine commands
    // is because they can act like a close if they're executed in the selected state, and it's not allowed
    // after a close as per RFC 3501 6.4.2
    symbolToInsert.sendUpdatedStatus = false;
    symbolToInsert.handler = examineHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("EXAMINE", symbolToInsert));
    symbolToInsert.handler = selectHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("SELECT", symbolToInsert));

    symbolToInsert.levels[0] = false;
    symbolToInsert.levels[1] = false;
    symbolToInsert.levels[2] = true;
    symbolToInsert.levels[3] = false;
    symbolToInsert.sendUpdatedStatus = true;
    symbolToInsert.handler = uidHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("UID", symbolToInsert));
    symbolToInsert.handler = checkHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("CHECK", symbolToInsert));
    symbolToInsert.handler = expungeHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("EXPUNGE", symbolToInsert));
    symbolToInsert.sendUpdatedStatus = false;
    symbolToInsert.handler = copyHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("COPY", symbolToInsert));
    symbolToInsert.handler = closeHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("CLOSE", symbolToInsert));
    symbolToInsert.handler = searchHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("SEARCH", symbolToInsert));
    symbolToInsert.handler = fetchHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("FETCH", symbolToInsert));
    symbolToInsert.handler = storeHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type("STORE", symbolToInsert));

    SearchHandler::buildSymbolTable();

    StatusHandler::buildSymbolTable();

    ParseBuffer::buildSymbolTable();

    uidSymbolTable.insert(UID_SUBCOMMAND_T::value_type("STORE",  UID_STORE));
    uidSymbolTable.insert(UID_SUBCOMMAND_T::value_type("FETCH",  UID_FETCH));
    uidSymbolTable.insert(UID_SUBCOMMAND_T::value_type("SEARCH", UID_SEARCH));
    uidSymbolTable.insert(UID_SUBCOMMAND_T::value_type("COPY",   UID_COPY));

    FetchHandler::buildSymbolTable();
}

ImapSession::ImapSession(ImapMaster *master, SessionDriver *driver, Server *server)
    : InternetSession(master, driver) {
    m_master = master;
    m_driver = driver;
    m_server = server;
    m_state = ImapNotAuthenticated;
    m_userData = NULL;
    m_loginDisabled = false;
    m_lineBuffer = NULL;	// going with a static allocation for now
    m_lineBuffPtr = 0;
    m_lineBuffLen = 0;
    m_store = NULL;

    m_lastCommandTime = time(NULL);
    m_purgedMessages.clear();
    m_retries = 0;
    if ((NULL != master) && (NULL != driver) && (NULL != server)) {
	std::string response("* OK [");
	response += capabilityString() + "] IMAP4rev1 server ready\r\n";
	m_driver->wantsToSend(response);

	m_failedLoginPause = m_master->badLoginPause();
	m_maxRetries = m_master->maxRetries();
	m_retryDelay = m_master->retryDelaySeconds();

	m_server->addTimerAction(new DeltaQueueIdleTimer(m_master->loginTimeout(), this));
	m_server->addTimerAction(new DeltaQueueAsynchronousAction(m_master->asynchronousEventTime(), this));
	m_driver->wantsToReceive();
    }
    m_currentHandler = NULL;
    m_savedParsingAt = 0;
}

ImapSession::~ImapSession() {
    if (ImapSelected == m_state) {
	m_store->mailboxClose();
    }
    if (NULL != m_store) {
	delete m_store;
    }
    m_store = NULL;
    if (NULL != m_userData) {
	delete m_userData;
    }
    m_userData = NULL;
    if ((NULL != m_driver) && (ImapLogoff != m_state)) {
	m_driver->wantsToSend("* BYE Wodin IMAP4rev1 server shutting down\r\n");
    }
}


/*--------------------------------------------------------------------------------------*/
/* AsynchronousEvent									*/
/*--------------------------------------------------------------------------------------*/
void ImapSession::asynchronousEvent(void) {
    if (ImapSelected == m_state) {
	NUMBER_SET purgedMessages;
	if (MailStore::SUCCESS == m_store->mailboxUpdateStats(&purgedMessages)) {
	    m_purgedMessages.insert(purgedMessages.begin(), purgedMessages.end());
	}
    }
}

/*--------------------------------------------------------------------------------------*/
/* ReceiveData										*/
/*--------------------------------------------------------------------------------------*/
void ImapSession::receiveData(uint8_t *data, size_t dataLen) {
    INPUT_DATA_STRUCT in_data;

    if (NULL == m_parseBuffer) {
	m_parseBuffer = new ParseBuffer(dataLen);
    }

    m_lastCommandTime = time(NULL);
  
    uint32_t newDataBase = 0;
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
		in_data.data = m_lineBuffer;
		in_data.dataLen = m_lineBuffPtr;
		in_data.parsingAt = 0;
		handleOneLine(in_data);
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
	if (0 < m_parseBuffer->literalLength()) {
	    size_t bytesToFeed = m_parseBuffer->literalLength();
	    if (bytesToFeed >= (dataLen - currentCommandStart)) {
		in_data.parsingAt = 0;
		in_data.data = &data[currentCommandStart];
		in_data.dataLen = dataLen - currentCommandStart;
		notDone = false;
		handleOneLine(in_data);
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
			in_data.parsingAt = 0;
			in_data.data = &data[currentCommandStart];
			in_data.dataLen = newDataBase - currentCommandStart + 1;
			handleOneLine(in_data);
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
		    in_data.parsingAt = 0;
		    in_data.data = &data[currentCommandStart];
		    in_data.dataLen = newDataBase - currentCommandStart + 1;
		    handleOneLine(in_data);
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
}

std::string ImapSession::formatTaggedResponse(IMAP_RESULTS status, bool sendUpdatedStatus) {
    std::string response;

    if ((status != IMAP_NOTDONE) && sendUpdatedStatus && (ImapSelected == m_state)) {
	for (NUMBER_SET::iterator i=m_purgedMessages.begin() ; i!= m_purgedMessages.end(); ++i) {
	    int message_number;
	    std::ostringstream ss;

	    message_number = m_store->mailboxUidToMsn(*i);
	    if (MailStore::SUCCESS == m_store->expungeThisUid(*i)) {
		ss << "* " << message_number << " EXPUNGE\r\n";
	    }
	    m_driver->wantsToSend(ss.str());
	}
	m_purgedMessages.clear();

	// I want to re-read the cached data if I'm in the selected state and
	// either the number of messages or the UIDNEXT value changes
	if ((m_currentNextUid != m_store->serialNumber()) ||
	    (m_currentMessageCount != m_store->mailboxMessageCount())) {
	    std::ostringstream ss;

	    if (m_currentNextUid != m_store->serialNumber()) {
		m_currentNextUid = m_store->serialNumber();
		ss << "* OK [UIDNEXT " << m_currentNextUid << "]\r\n";
	    }
	    if (m_currentMessageCount != m_store->mailboxMessageCount()) {
		m_currentMessageCount = m_store->mailboxMessageCount();
		ss << "* " << m_currentMessageCount << " EXISTS\r\n";
	    }
	    if (m_currentRecentCount != m_store->mailboxRecentCount()) {
		m_currentRecentCount = m_store->mailboxRecentCount();
		ss << "* " << m_currentRecentCount << " RECENT\r\n";
	    }
	    if (m_currentUnseen != m_store->mailboxFirstUnseen()) {
		m_currentUnseen = m_store->mailboxFirstUnseen();
		if (0 < m_currentUnseen) {
		    ss << "* OK [UNSEEN " << m_currentUnseen << "]\r\n";
		}
	    }
	    if (m_currentUidValidity != m_store->uidValidityNumber()) {
		m_currentUidValidity = m_store->uidValidityNumber();
		ss << "* OK [UIDVALIDITY " << m_currentUidValidity << "]\r\n";
	    }
	    m_driver->wantsToSend(ss.str());
	}
    }

    switch(status) {
    case IMAP_OK:
	response = m_parseBuffer->tag();
	response += " OK";
	response += m_responseCode[0] == '\0' ? "" : " ";
	response += (char *)m_responseCode;
	response += " ";
	response += m_parseBuffer->command();
	response += " ";
	response += (char *)m_responseText;
	response += "\r\n";
	break;
 
    case IMAP_NO:
    case IMAP_NO_WITH_PAUSE:
	response = m_parseBuffer->tag();
	response += " NO";
	response += m_responseCode[0] == '\0' ? "" : " ";
	response += (char *)m_responseCode;
	response += " ";
	response += m_parseBuffer->command();
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
	response = m_parseBuffer->tag();
	response += " NO ";
	response += m_parseBuffer->command();
	response += " \"";
	response += m_store->turnErrorCodeIntoString(m_mboxErrorCode);
	response += "\"\r\n";
	break;

    case IMAP_IN_LITERAL:
	response = "";
	break;

    default:
    case IMAP_BAD:
	response = m_parseBuffer->tag();
	response += " BAD";
	response += m_responseCode[0] == '\0' ? "" : " ";
	response += (char *)m_responseCode;
	response += " ";
	response += m_parseBuffer->command();
	response += " ";
	response += (char *)m_responseText;
	response += "\r\n";
	break; 
    }

    return response;
}

// This method is called to retry a locking attempt
void ImapSession::doRetry(void) {
    uint8_t t[1];
    INPUT_DATA_STRUCT in_data;
    in_data.data = t;
    in_data.dataLen = 0;
    in_data.parsingAt = 0;

    handleOneLine(in_data);
}

// This method is called to retry a locking attempt
void ImapSession::idleTimeout(void) {
    m_state = ImapLogoff;
    m_driver->wantsToSend("* BYE Idle timeout disconnect\r\n");
    m_server->killSession(m_driver);
}

// This function assumes that it is passed a single entire line each time.
// It returns true if it's expecting more data, and false otherwise
void ImapSession::handleOneLine(INPUT_DATA_STRUCT &input) {
    std::string response;
    size_t i;
    bool commandFound = false;

    if (NULL == m_currentHandler) {
	m_parseStage = 0;
	input.dataLen -= 2;  // It's not literal data, so I can strip the CRLF off the end
	input.data[input.dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	// std::cout << "In HandleOneLine, new command \"" << data << "\"" << std::endl;
	// There's nothing magic about 10, it just seems a convenient size
	// The initial value of parseBuffLen should be bigger than dwDataLen because
	// if it's the same size or smaller, it WILL have to be reallocated at some point.
	// m_parseBuffer = new ParseBuffer(dataLen);

	char *tagEnd = strchr((char *)input.data, ' ');
	if (NULL != tagEnd) {
	    i = (size_t) (tagEnd - ((char *)input.data));
	}
	else {
	    i = input.dataLen;
	}
	m_parseBuffer->addToParseBuffer(input, i);
	while ((input.parsingAt < input.dataLen) && (' ' == input.data[input.parsingAt])) {
	    ++input.parsingAt;  // Lose the space separating the tag from the command
	}
	if (i < input.dataLen) {
	    commandFound = true;
	    m_parseBuffer->commandHere();
	    tagEnd = strchr((char *)&input.data[input.parsingAt], ' ');
	    if (NULL != tagEnd) {
		m_parseBuffer->addToParseBuffer(input, (size_t) (tagEnd - ((char *)&input.data[input.parsingAt])));
	    }
	    else {
		m_parseBuffer->addToParseBuffer(input, input.dataLen - i);
	    }

	    m_parseBuffer->argumentsHere();
	    while ((input.parsingAt < input.dataLen) && (' ' == input.data[input.parsingAt])) {
		++input.parsingAt;  // Lose the space between the command and the arguments
	    }
	    // std::cout << "Looking up the command \"" << &m_parseBuffer[m_commandString] << "\"" << std::endl;
	    IMAPSYMBOLS::iterator found = m_symbols.find(m_parseBuffer->command());
	    if ((found != m_symbols.end()) && found->second.levels[m_state]) {
		m_currentHandler = found->second.handler(this, input);
		m_sendUpdatedStatus = found->second.sendUpdatedStatus;
	    }
	}
	m_savedParsingAt = input.parsingAt;
    }
    if (NULL != m_currentHandler) {
	IMAP_RESULTS status;
	m_responseCode[0] = '\0';
	input.parsingAt = m_savedParsingAt;
	strncpy(m_responseText, "Completed", MAX_RESPONSE_STRING_LENGTH);
	status = m_currentHandler->receiveData(input);
	m_savedParsingAt = input.parsingAt;
	response = formatTaggedResponse(status, m_sendUpdatedStatus);
	if ((IMAP_NOTDONE != status) && (IMAP_IN_LITERAL != status) && (IMAP_TRY_AGAIN != status)) {
	    delete m_currentHandler;
	    m_currentHandler = NULL;
	}
	switch (status) {
	case IMAP_NO_WITH_PAUSE:
	    m_retries = 0;
	    m_server->addTimerAction(new DeltaQueueDelayedMessage(m_failedLoginPause, this, response));
	    break;

	case IMAP_TRY_AGAIN:
	    if (m_retries < m_maxRetries) {
		m_retries++;
		m_server->addTimerAction(new DeltaQueueRetry(m_retryDelay, this));
	    }
	    else {
		strncpy(m_responseText, "Locking Error:  Too Many Retries", MAX_RESPONSE_STRING_LENGTH);
		response = formatTaggedResponse(IMAP_NO, false);
		m_driver->wantsToSend(response);
		m_retries = 0;
		m_driver->wantsToReceive();
		delete m_currentHandler;
		m_currentHandler = NULL;
	    }
	    break;

	default:
	    m_retries = 0;
	    if (IMAP_IN_LITERAL != status) {
		m_driver->wantsToSend(response);
	    }
	    m_driver->wantsToReceive();
	    break;
	}
    }
    else {
	if (commandFound) {
	    strncpy(m_responseText, "Unrecognized Command", MAX_RESPONSE_STRING_LENGTH);
	    m_responseCode[0] = '\0';
	    response = formatTaggedResponse(IMAP_BAD, true);
	    m_driver->wantsToSend(response);
	}
	else {
	    response = m_parseBuffer->tag();
	    response += " BAD No Command\r\n";
	    m_driver->wantsToSend(response);
	}
	m_driver->wantsToReceive();
    }
    if (ImapLogoff <= m_state) {
	// If I'm logged off, I'm obviously not expecting any more data
	m_server->killSession(m_driver);
    }
    if (NULL == m_currentHandler) {
	if (NULL != m_parseBuffer) {
	    delete m_parseBuffer;
	    m_parseBuffer = NULL;
	}
    }
}


std::string ImapSession::capabilityString() {
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


void ImapSession::closeMailbox(ImapState newState) {
    if (ImapSelected == m_state) {
	m_store->mailboxClose();
    }
    if (ImapNotAuthenticated < m_state) {
	if (NULL != m_store) {
	    delete m_store;
	    m_store = NULL;
	}
    }
    m_state = newState;
}

void ImapSession::responseText(void) {
    m_responseText[0] = '\0';
}

void ImapSession::responseText(const std::string &msg) {
    strncpy(m_responseText, msg.c_str(),  MAX_RESPONSE_STRING_LENGTH);
}

void ImapSession::responseCode(const std::string &msg) {
    strncpy(m_responseCode, msg.c_str(), MAX_RESPONSE_STRING_LENGTH);
}

void ImapSession::selectData(const std::string &mailbox, bool isReadWrite) {
    (void) mailbox;

    std::ostringstream ss;
    m_currentNextUid = m_store->serialNumber();
    m_currentUidValidity = m_store->uidValidityNumber();
    m_currentMessageCount = m_store->mailboxMessageCount();
    m_currentRecentCount = m_store->mailboxRecentCount();
    m_currentUnseen = m_store->mailboxFirstUnseen();

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
    m_driver->wantsToSend(ss.str());
}


// A sequence set is a sequence of range values, each one of which is separated by commas
// A range value is one of a number, a star, or two of either of those separated by 
// a colon.  A star represents the last value in the range.  Two values separated by
// a colon represent the entire range of valid numbers between and including those two
// values.  A star is always valid unless the mail store has no messages in it.
//
// The resulting list of values must be sorted or nothing else works
bool ImapSession::msnSequenceSet(SEARCH_RESULT &uidVector, size_t &tokenPointer) {
    bool result = true;

    size_t mboxCount = m_store->mailboxMessageCount();
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
		uidVector.push_back(m_store->mailboxMsnToUid(i));
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

bool ImapSession::uidSequenceSet(SEARCH_RESULT &uidVector, size_t &tokenPointer) {
    bool result = true;

    size_t mboxCount = m_store->mailboxMessageCount();
    size_t mboxMaxUid;
    if (0 != mboxCount) {
	mboxMaxUid = m_store->mailboxMsnToUid(m_store->mailboxMessageCount());
    }
    else {
	mboxMaxUid = 0; 
    }
    size_t mboxMinUid;
    if (0 != mboxCount) {
	mboxMinUid = m_store->mailboxMsnToUid(1);
    }
    else {
	mboxMinUid = 0;
    }
    char *s, *e;
    s = (char *)&m_parseBuffer[tokenPointer];
    do {
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
	    if (0 < m_store->mailboxUidToMsn(i)) {
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

void ImapSession::fetchResponseUid(unsigned long uid) {
    std::ostringstream result;
    result << "UID " << uid;
    m_driver->wantsToSend(result.str());
}

void ImapSession::fetchResponseFlags(uint32_t flags) {
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
    m_driver->wantsToSend(result);
}

