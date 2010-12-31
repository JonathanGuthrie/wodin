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

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

// #include <iostream>
#include <list>

typedef enum {
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

typedef enum {
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

typedef enum {
  UID_STORE,
  UID_FETCH,
  UID_SEARCH,
  UID_COPY
} UID_SUBCOMMAND_VALUES;
typedef std::map<std::string, UID_SUBCOMMAND_VALUES> UID_SUBCOMMAND_T;

static UID_SUBCOMMAND_T uidSymbolTable;

typedef enum {
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

typedef enum {
  FETCH_BODY_BODY,
  FETCH_BODY_HEADER,
  FETCH_BODY_FIELDS,
  FETCH_BODY_NOT_FIELDS,
  FETCH_BODY_TEXT,
  FETCH_BODY_MIME
} FETCH_BODY_PARTS;

bool ImapSession::m_anonymousEnabled = false;

IMAPSYMBOLS ImapSession::m_symbols;

void ImapSession::buildSymbolTables() {
  MailMessage::buildSymbolTable();

  symbol symbolToInsert;
  symbolToInsert.levels[0] = true;
  symbolToInsert.levels[1] = true;
  symbolToInsert.levels[2] = true;
  symbolToInsert.levels[3] = true;
  symbolToInsert.sendUpdatedStatus = true;
  symbolToInsert.flag = false;
  symbolToInsert.handler = &ImapSession::capabilityHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("CAPABILITY", symbolToInsert));
  symbolToInsert.handler = &ImapSession::noopHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("NOOP", symbolToInsert));
  symbolToInsert.handler = &ImapSession::logoutHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("LOGOUT", symbolToInsert));

  symbolToInsert.levels[0] = true;
  symbolToInsert.levels[1] = false;
  symbolToInsert.levels[2] = false;
  symbolToInsert.levels[3] = false;
  symbolToInsert.sendUpdatedStatus = true;
  symbolToInsert.handler = &ImapSession::starttlsHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("STARTTTLS", symbolToInsert));
  symbolToInsert.handler = &ImapSession::authenticateHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("AUTHENTICATE", symbolToInsert));
  symbolToInsert.handler = &ImapSession::loginHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("LOGIN", symbolToInsert));

  symbolToInsert.levels[0] = false;
  symbolToInsert.levels[1] = true;
  symbolToInsert.levels[2] = true;
  symbolToInsert.levels[3] = false;
  symbolToInsert.sendUpdatedStatus = true;
  symbolToInsert.handler = &ImapSession::namespaceHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("NAMESPACE", symbolToInsert));
  symbolToInsert.handler = &ImapSession::createHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("CREATE", symbolToInsert));
  symbolToInsert.handler = &ImapSession::deleteHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("DELETE", symbolToInsert));
  symbolToInsert.handler = &ImapSession::renameHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("RENAME", symbolToInsert));
  symbolToInsert.handler = &ImapSession::subscribeHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("UNSUBSCRIBE", symbolToInsert));
  symbolToInsert.flag = true;
  symbolToInsert.handler = &ImapSession::subscribeHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("SUBSCRIBE", symbolToInsert));
  symbolToInsert.handler = &ImapSession::listHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("LIST", symbolToInsert));
  symbolToInsert.flag = false;
  m_symbols.insert(IMAPSYMBOLS::value_type("LSUB", symbolToInsert));
  symbolToInsert.handler = &ImapSession::statusHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("STATUS", symbolToInsert));
  symbolToInsert.handler = &ImapSession::appendHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("APPEND", symbolToInsert));
  // My justification for not allowing the updating of the status after the select and examine commands
  // is because they can act like a close if they're executed in the selected state, and it's not allowed
  // after a close as per RFC 3501 6.4.2
  symbolToInsert.sendUpdatedStatus = false;
  symbolToInsert.handler = &ImapSession::selectHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("EXAMINE", symbolToInsert));
  symbolToInsert.flag = true;
  symbolToInsert.handler = &ImapSession::selectHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("SELECT", symbolToInsert));

  symbolToInsert.levels[0] = false;
  symbolToInsert.levels[1] = false;
  symbolToInsert.levels[2] = true;
  symbolToInsert.levels[3] = false;
  symbolToInsert.sendUpdatedStatus = true;
  symbolToInsert.handler = &ImapSession::uidHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("UID", symbolToInsert));
  symbolToInsert.flag = false;
  symbolToInsert.handler = &ImapSession::checkHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("CHECK", symbolToInsert));
  symbolToInsert.handler = &ImapSession::expungeHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("EXPUNGE", symbolToInsert));
  symbolToInsert.sendUpdatedStatus = false;
  symbolToInsert.handler = &ImapSession::copyHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("COPY", symbolToInsert));
  symbolToInsert.handler = &ImapSession::closeHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("CLOSE", symbolToInsert));
  symbolToInsert.handler = &ImapSession::searchHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("SEARCH", symbolToInsert));
  symbolToInsert.handler = &ImapSession::fetchHandler;
  m_symbols.insert(IMAPSYMBOLS::value_type("FETCH", symbolToInsert));
  symbolToInsert.handler = &ImapSession::storeHandler;
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

ImapSession::ImapSession(ImapMaster *master, SessionDriver *driver, Server *server)
  : InternetSession(master, driver) {
  m_master = master;
  m_driver = driver;
  m_server = server;
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
  response += capabilityString() + "] IMAP4rev1 server ready\r\n";
  m_driver->wantsToSend(response);
  m_lastCommandTime = time(NULL);
  m_purgedMessages.clear();
  m_retries = 0;

  m_failedLoginPause = m_master->badLoginPause();
  m_maxRetries = m_master->maxRetries();
  m_retryDelay = m_master->retryDelaySeconds();

  m_server->addTimerAction(new DeltaQueueIdleTimer(m_master->loginTimeout(), this));
  m_server->addTimerAction(new DeltaQueueAsynchronousAction(m_master->asynchronousEventTime(), this));
  m_driver->wantsToReceive();
  m_currentHandler = NULL;
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
  if (ImapLogoff != m_state) {
    m_driver->wantsToSend("* BYE Wodin IMAP4rev1 server shutting down\r\n");
  }
}


void ImapSession::atom(uint8_t *data, size_t dataLen, size_t &parsingAt) {
  static char include_list[] = "!#$&'+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[^_`abcdefghijklmnopqrstuvwxyz|}~";
 
  while (parsingAt < dataLen) {
    if (NULL != strchr(include_list, data[parsingAt])) {
      uint8_t temp = toupper(data[parsingAt]);
      addToParseBuffer(&temp, 1, false);
    }
    else {
      addToParseBuffer(NULL, 0);
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
	    addToParseBuffer(&data[parsingAt], 1, false);
	    escape_flag = false;
	  }
	  else {
	    addToParseBuffer(NULL, 0);
	    notdone = false;
	  }
	  break;

	case '\\':
	  if (escape_flag) {
	    addToParseBuffer(&data[parsingAt], 1, false);
	  }
	  escape_flag = !escape_flag;
	  break;

	default:
	  escape_flag = false;
	  if (makeUppercase) {
	    uint8_t temp = toupper(data[parsingAt]);
	    addToParseBuffer(&temp, 1, false);
	  }
	  else {
	    addToParseBuffer(&data[parsingAt], 1, false);
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
	    addToParseBuffer(&temp, 1, false);
	  }
	  else {
	    addToParseBuffer(&data[parsingAt], 1, false);
	  }
	  ++parsingAt;
	}
	addToParseBuffer(NULL, 0);
      }
    }
  }
  return result;
}


void ImapSession::addToParseBuffer(const uint8_t *data, size_t length, bool nulTerminate) {
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
	handleOneLine(m_lineBuffer, m_lineBuffPtr);
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
	handleOneLine(&data[currentCommandStart], bytesToFeed);
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
	    handleOneLine(&data[currentCommandStart], newDataBase - currentCommandStart + 1);
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
	  handleOneLine(&data[currentCommandStart], newDataBase - currentCommandStart + 1);
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
      ss << "* " << message_number << " EXPUNGE\r\n";
      m_store->expungeThisUid(*i);
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
    response += m_store->turnErrorCodeIntoString(m_mboxErrorCode);
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

// This method is called to retry a locking attempt
void ImapSession::doRetry(void) {
  uint8_t t[1];

  handleOneLine(t, 0);
}

// This method is called to retry a locking attempt
void ImapSession::idleTimeout(void) {
  m_state = ImapLogoff;
  m_driver->wantsToSend("* BYE Idle timeout disconnect\r\n");
  m_server->killSession(m_driver);
}

// This function assumes that it is passed a single entire line each time.
// It returns true if it's expecting more data, and false otherwise
void ImapSession::handleOneLine(uint8_t *data, size_t dataLen) {
  std::string response;
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
    addToParseBuffer(data, i);
    while ((i < dataLen) && (' ' == data[i])) {
      ++i;  // Lose the space separating the tag from the command
    }
    if (i < dataLen) {
      commandFound = true;
      m_commandString = m_parsePointer;
      tagEnd = strchr((char *)&data[i], ' ');
      if (NULL != tagEnd) {
	addToParseBuffer(&data[i], (size_t) (tagEnd - ((char *)&data[i])));
	i = (size_t) (tagEnd - ((char *)data));
      }
      else {
	addToParseBuffer(&data[i], dataLen - i);
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
    response = formatTaggedResponse(status, m_sendUpdatedStatus);
    if ((IMAP_NOTDONE != status) && (IMAP_IN_LITERAL != status) && (IMAP_TRY_AGAIN != status)) {
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
      response = (char *)m_parseBuffer;
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
    m_literalLength = 0;
    if (NULL != m_parseBuffer) {
      delete[] m_parseBuffer;
      m_parseBuffer = NULL;
      m_parseBuffLen = 0;
    }
  }
}


/*--------------------------------------------------------------------------------------*/
/* ImapSession::unimplementedHandler													*/
/*--------------------------------------------------------------------------------------*/
/* Gets called whenever the system doesn't know what else to do.  It signals the client */
/* that the command was not recognized													*/
/*--------------------------------------------------------------------------------------*/
IMAP_RESULTS ImapSession::unimplementedHandler() {
  strncpy(m_responseText, "Unrecognized Command",  MAX_RESPONSE_STRING_LENGTH);
  return IMAP_BAD;
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


/*
 * The capability handler looks simple, but there's some complexity lurking
 * there.  For starters, I'm going to return a constant string with not very
 * much in it.  However, later on the string might change depending on the
 * context.  Further, since the capability string might be sent by multiple
 * different commands, that actual work will be done by a worker function.
 */
IMAP_RESULTS ImapSession::capabilityHandler(uint8_t *pData, size_t dataLen, size_t &r_dwParsingAt, bool unused) {
  std::string response("* ");
  response += capabilityString() + "\r\n";
  m_driver->wantsToSend(response);
  return IMAP_OK;
}

/*
 * The NOOP may be used by the server to trigger things like checking for
 * new messages although the server doesn't have to wait for this to do
 * things like that
 */
IMAP_RESULTS ImapSession::noopHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
  // This command literally doesn't do anything.  If there was an update, it was found
  // asynchronously and the updated info will be printed in formatTaggedResponse

  return IMAP_OK;
}


/*
 * The LOGOUT indicates that the session may be closed.  The main command
 * processor gets the word about this because the state is set to "4" which
 * means "logout state"
 */
IMAP_RESULTS ImapSession::logoutHandler(uint8_t *pData, size_t dataLen, size_t &r_dwParsingAt, bool unused) {
  // If the mailbox is open, close it
  // In IMAP, deleted messages are always purged before a close
  if (ImapSelected == m_state) {
    m_store->mailboxClose();
  }
  if (ImapNotAuthenticated < m_state) {
    if (NULL != m_store) {
      delete m_store;
      m_store = NULL;
    }
  }
  m_state = ImapLogoff;
  m_driver->wantsToSend("* BYE IMAP4rev1 server closing\r\n");
  return IMAP_OK;
}

IMAP_RESULTS ImapSession::starttlsHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
  return unimplementedHandler();
}

IMAP_RESULTS ImapSession::authenticateHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
  IMAP_RESULTS result; 
  switch (m_parseStage) {
  case 0:
    m_userData = m_master->userInfo((char *)&m_parseBuffer[m_arguments]);
    atom(data, dataLen, parsingAt);
    m_auth = SaslChooser(m_master, (char *)&m_parseBuffer[m_arguments]);
    if (NULL != m_auth) {
      m_auth->sendChallenge(m_responseCode);
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

    switch (m_auth->receiveResponse(str)) {
    case Sasl::ok:
      result = IMAP_OK;
      // user->GetUserFileSystem();
      m_userData = m_master->userInfo(m_auth->getUser().c_str());

      if (NULL == m_store) {
	m_store = m_master->mailStore(this);
      }

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


IMAP_RESULTS ImapSession::loginHandlerExecute() {
  IMAP_RESULTS result;

  m_userData = m_master->userInfo((char *)&m_parseBuffer[m_arguments]);
  bool v = m_userData->checkCredentials((char *)&m_parseBuffer[m_arguments+(strlen((char *)&m_parseBuffer[m_arguments])+1)]);
  if (v) {
    m_responseText[0] = '\0';
    // SYZYGY LOG
    // Log("Client %u logged-in user %s %lu\n", m_dwClientNumber, m_pUser->m_szUsername.c_str(), m_pUser->m_nUserID);
    if (NULL == m_store) {
      m_store = m_master->mailStore(this);
    }
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
 * to use the AUTHENTICATE command with some SASL mechanism.  I will include it,
 * but I'll also include a check of the login_disabled flag, which will set whether or
 * not this command is accepted by the command processor.  Eventually, the master will
 * know whether or not to accept the login command.
 */
IMAP_RESULTS ImapSession::loginHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
  IMAP_RESULTS result;

  if (m_LoginDisabled) {
    strncpy(m_responseText, "Login Disabled", MAX_RESPONSE_STRING_LENGTH);
    return IMAP_NO;
  }
  else {
    result = IMAP_OK;
    if (0 < m_literalLength) {
      if (dataLen >= m_literalLength) {
	addToParseBuffer(data, m_literalLength);
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
	addToParseBuffer(data, dataLen, false);
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
      result = loginHandlerExecute();
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


IMAP_RESULTS ImapSession::namespaceHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
  // SYZYGY parsingAt should be the index of a '\0'
  std::string str = "* NAMESPACE " + m_store->listNamespaces();
  str += "\n";
  m_driver->wantsToSend(str);
  return IMAP_OK;
}


void ImapSession::selectData(const std::string &mailbox, bool isReadWrite) {
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


IMAP_RESULTS ImapSession::selectHandlerExecute(bool openReadWrite) {
  IMAP_RESULTS result = IMAP_MBOX_ERROR;

  if (ImapSelected == m_state) {
    // If the mailbox is open, close it
    if (MailStore::CANNOT_COMPLETE_ACTION == m_store->mailboxClose()) {
      result = IMAP_TRY_AGAIN;
    }
    else {
      m_state = ImapAuthenticated;
    }
  }
  if (ImapAuthenticated == m_state) {
    std::string mailbox = (char *)&m_parseBuffer[m_arguments];
    switch (m_mboxErrorCode = m_store->mailboxOpen(mailbox, openReadWrite)) {
    case MailStore::SUCCESS:
      selectData(mailbox, openReadWrite);
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
IMAP_RESULTS ImapSession::selectHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool isReadOnly) {
  IMAP_RESULTS result = IMAP_OK;

  switch (m_parseStage) {
  case 0:
    switch (astring(data, dataLen, parsingAt, false, NULL)) {
    case ImapStringGood:
      m_parseStage = 2;
      result = selectHandlerExecute(isReadOnly);
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
      addToParseBuffer(data, m_literalLength);
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
      result = selectHandlerExecute(isReadOnly);
    }
    else {
      result = IMAP_IN_LITERAL;
      addToParseBuffer(data, dataLen, false);
      m_literalLength -= dataLen;
    }
    break;

  default:
    result = selectHandlerExecute(isReadOnly);
    break;
  }

  return result;
}


IMAP_RESULTS ImapSession::createHandlerExecute() {
  IMAP_RESULTS result = IMAP_MBOX_ERROR;
  // SYZYGY -- check to make sure that the argument list has just the one argument
  // SYZYGY -- parsingAt should point to '\0'
  std::string mailbox((char *)&m_parseBuffer[m_arguments]);
  switch (m_mboxErrorCode = m_store->createMailbox(mailbox)) {
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
IMAP_RESULTS ImapSession::createHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
  IMAP_RESULTS result = IMAP_OK;

  switch (m_parseStage) {
  case 0:
    switch (astring(data, dataLen, parsingAt, false, NULL)) {
    case ImapStringGood:
      m_parseStage = 2;
      result = createHandlerExecute();
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
      addToParseBuffer(data, m_literalLength);
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
      result = createHandlerExecute();
    }
    else {
      result = IMAP_IN_LITERAL;
      addToParseBuffer(data, dataLen, false);
      m_literalLength -= dataLen;
    }
    break;

  default:
    result = createHandlerExecute();
    break;
  }
  return result;
}

IMAP_RESULTS ImapSession::deleteHandlerExecute() {
  IMAP_RESULTS result = IMAP_MBOX_ERROR;
  // SYZYGY -- check to make sure that the argument list has just the one argument
  // SYZYGY -- parsingAt should point to '\0'
  std::string mailbox((char *)&m_parseBuffer[m_arguments]);
  switch (m_mboxErrorCode = m_store->deleteMailbox(mailbox)) {
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
IMAP_RESULTS ImapSession::deleteHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
  IMAP_RESULTS result = IMAP_OK;

  switch (m_parseStage) {
  case 0:
    // SYZYGY -- check to make sure that the argument list has just the one argument
    switch (astring(data, dataLen, parsingAt, false, NULL)) {
    case ImapStringGood:
      m_parseStage = 2;
      result = deleteHandlerExecute();
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
      addToParseBuffer(data, m_literalLength);
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
      result = deleteHandlerExecute();
    }
    else {
      result = IMAP_IN_LITERAL;
      addToParseBuffer(data, dataLen, false);
      m_literalLength -= dataLen;
    }
    break;

  default:
    result = deleteHandlerExecute();
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
IMAP_RESULTS ImapSession::renameHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
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
	addToParseBuffer(data, m_literalLength);
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
	addToParseBuffer(data, dataLen, false);
	m_literalLength -= dataLen;
      }
      break;

    default: {
      m_parseStage = 5;
      std::string source((char *)&m_parseBuffer[m_arguments]);
      std::string destination((char *)&m_parseBuffer[m_arguments+(strlen((char *)&m_parseBuffer[m_arguments])+1)]);

      switch (m_mboxErrorCode = m_store->renameMailbox(source, destination)) {
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
  } while ((IMAP_OK == result) && (m_parseStage < 5));

  return result;
}

IMAP_RESULTS ImapSession::subscribeHandlerExecute(bool isSubscribe) {
  IMAP_RESULTS result = IMAP_MBOX_ERROR;
  // SYZYGY -- check to make sure that the argument list has just the one argument
  // SYZYGY -- parsingAt should point to '\0'
  std::string mailbox((char *)&m_parseBuffer[m_arguments]);

  switch (m_mboxErrorCode = m_store->subscribeMailbox(mailbox, isSubscribe)) {
  case MailStore::SUCCESS:
    result = IMAP_OK;
    break;

  case MailStore::CANNOT_COMPLETE_ACTION:
    result = IMAP_TRY_AGAIN;
    break;
  }
  return result;
}

IMAP_RESULTS ImapSession::subscribeHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool isSubscribe) {
  IMAP_RESULTS result = IMAP_OK;

  switch (m_parseStage) {
  case 0:
    switch (astring(data, dataLen, parsingAt, false, NULL)) {
    case ImapStringGood:
      result = subscribeHandlerExecute(isSubscribe);
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
      addToParseBuffer(data, m_literalLength);
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
      result = subscribeHandlerExecute(isSubscribe);
    }
    else {
      result = IMAP_IN_LITERAL;
      addToParseBuffer(data, dataLen, false);
      m_literalLength -= dataLen;
    }
    break;

  default:
    result = subscribeHandlerExecute(isSubscribe);
  }
  return result;
}


static std::string imapQuoteString(const std::string &to_be_quoted) {
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
static std::string genMailboxFlags(const uint32_t attr) {
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

IMAP_RESULTS ImapSession::listHandlerExecute(bool listAll) {
  IMAP_RESULTS result = IMAP_OK;

  std::string reference = (char *)&m_parseBuffer[m_arguments];
  std::string mailbox = (char *)&m_parseBuffer[m_arguments+(strlen((char *)&m_parseBuffer[m_arguments])+1)];

  if ((0 == reference.size()) && (0 == mailbox.size())) {
    if (listAll) {
      // It's a request to return the base and the hierarchy delimiter
      m_driver->wantsToSend("* LIST () \"/\" \"\"\r\n");
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
    m_store->mailboxList(reference, &mailboxList, listAll);
    std::string response;
    for (MAILBOX_LIST::const_iterator i = mailboxList.begin(); i != mailboxList.end(); ++i) {
      if (listAll) {
	response = "* LIST ";
      }
      else {
	response = "* LSUB ";
      }
      MAILBOX_NAME which = *i;
      response += genMailboxFlags(i->attributes) + " \"/\" " + imapQuoteString(i->name) + "\r\n";
      m_driver->wantsToSend(response);
    }
  }
  return IMAP_OK;
}

IMAP_RESULTS ImapSession::listHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool listAll) {
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
	result = listHandlerExecute(listAll);
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
      addToParseBuffer(data, m_literalLength);
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
	  result = listHandlerExecute(listAll);
	}
	else {
	  strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	  result = IMAP_BAD;
	}
      }
    }
    else {
      result = IMAP_IN_LITERAL;
      addToParseBuffer(data, dataLen, false);
      m_literalLength -= dataLen;
    }
  }
  return result;
}


IMAP_RESULTS ImapSession::statusHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
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
      addToParseBuffer(data, m_literalLength);
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
      addToParseBuffer(data, dataLen, false);
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

    switch (m_mboxErrorCode = m_store->getMailboxCounts(mailbox, m_mailFlags, messageCount, recentCount, uidNext, uidValidity, firstUnseen)) {
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
	m_driver->wantsToSend(response.str());
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

size_t ImapSession::readEmailFlags(uint8_t *data, size_t dataLen, size_t &parsingAt, bool &okay) {
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

IMAP_RESULTS ImapSession::appendHandlerExecute(uint8_t *data, size_t dataLen, size_t &parsingAt) {
  IMAP_RESULTS result = IMAP_BAD;
  DateTime messageDateTime;

  if ((2 < (dataLen - parsingAt)) && (' ' == data[parsingAt++])) {
    if ('(' == data[parsingAt]) {
      ++parsingAt;
      bool flagsOk;
      m_mailFlags = readEmailFlags(data, dataLen, parsingAt, flagsOk);
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
      if (messageDateTime.parse(data, dataLen, parsingAt, DateTime::IMAP) &&
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
		
	switch (m_store->addMessageToMailbox(mailbox, m_lineBuffer, 0, messageDateTime, m_mailFlags, &m_appendingUid)) {
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

IMAP_RESULTS ImapSession::appendHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
  IMAP_RESULTS result = IMAP_OK;

  switch (m_parseStage) {
  case 0:
    switch (astring(data, dataLen, parsingAt, false, NULL)) {
    case ImapStringGood:
      result = appendHandlerExecute(data, dataLen, parsingAt);
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
      addToParseBuffer(data, m_literalLength);
      ++m_parseStage;
      size_t i = m_literalLength;
      m_literalLength = 0;
      if (2 < dataLen) {
	// Get rid of the CRLF if I have it
	dataLen -= 2;
	data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
      }
      m_responseText[0] = '\0';
      result = appendHandlerExecute(data, dataLen, i);
    }
    else {
      result = IMAP_IN_LITERAL;
      addToParseBuffer(data, m_literalLength, false);
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
      switch (m_store->appendDataToMessage(mailbox, m_appendingUid, data, len)) {
      case MailStore::SUCCESS:
	m_literalLength -= len;
	if (0 == m_literalLength) {
	  m_store->doneAppendingDataToMessage(mailbox, m_appendingUid);
	  m_parseStage = 3;
	  m_appendingUid = 0;
	}
	break;

      case MailStore::CANNOT_COMPLETE_ACTION:
	result = IMAP_TRY_AGAIN;
	break;

      default:
	m_store->doneAppendingDataToMessage(mailbox, m_appendingUid);
	m_store->deleteMessage(mailbox, m_appendingUid);
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

IMAP_RESULTS ImapSession::checkHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
  // Unlike NOOP, I always call mailboxFlushBuffers because that recalculates the the cached data.
  // That may be the only way to find out that the number of messages or the UIDNEXT value has
  // changed.
  IMAP_RESULTS result = IMAP_OK;
  NUMBER_SET purgedMessages;

  if (MailStore::SUCCESS == m_store->mailboxUpdateStats(&purgedMessages)) {
    m_purgedMessages.insert(purgedMessages.begin(), purgedMessages.end());
  }
  m_store->mailboxFlushBuffers();

  return result;
}

IMAP_RESULTS ImapSession::closeHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
  // If the mailbox is open, close it
  // In IMAP, deleted messages are always purged before a close
  IMAP_RESULTS result = IMAP_TRY_AGAIN;
  if (MailStore::SUCCESS == m_store->lock()) {
    NUMBER_SET purgedMessages;
    m_store->listDeletedMessages(&purgedMessages);
    m_purgedMessages.insert(purgedMessages.begin(), purgedMessages.end());
    for(NUMBER_SET::iterator i=purgedMessages.begin(); i!=purgedMessages.end(); ++i) {
      m_store->expungeThisUid(*i);
    }
    m_store->mailboxClose();
    m_state = ImapAuthenticated;
    m_store->unlock();
    result = IMAP_OK;
  }
  return result;
}

IMAP_RESULTS ImapSession::expungeHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool unused) {
  IMAP_RESULTS result = IMAP_MBOX_ERROR;
  NUMBER_SET purgedMessages;

  switch (m_mboxErrorCode = m_store->listDeletedMessages(&purgedMessages)) {
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

// updateSearchTerm updates the search specification for one term.  That term may be a 
// parenthesized list of other terms, in which case it calls updateSearchTerms to evaluate
// all of those.  updateSearchTerms will, in turn, call updateSearchTerm to update the 
// searchTerm for each of the terms in the list.
// updateSearchTerm return true if everything went okay or false if there were errors in
// the Search Specification String.
bool ImapSession::updateSearchTerm(MailSearch &searchTerm, size_t &tokenPointer) {
  bool result = true;

  if ('(' == m_parseBuffer[tokenPointer]) {
    tokenPointer += 2;
    result = updateSearchTerms(searchTerm, tokenPointer, true);
  }
  else {
    if (('*' == m_parseBuffer[tokenPointer]) || isdigit(m_parseBuffer[tokenPointer])) {
      // It's a list of MSN ranges
      // How do I treat a list of numbers?  Well, I generate a vector of all the possible values and then 
      // and it (do a "set intersection" between it and the current vector, if any.

      // I don't need to check to see if tokenPointer is less than m_parsePointer because this function
      // wouldn't get called if it wasn't
      SEARCH_RESULT srVector;
      result = msnSequenceSet(srVector, tokenPointer);
      searchTerm.addUidVector(srVector);
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
	  searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_ANSWERED);
	  break;

	case SSV_BCC:
	  if (tokenPointer < m_parsePointer) {
	    searchTerm.addBccSearch((char *)&m_parseBuffer[tokenPointer]);
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
	    if (date.isValid()) {
	      tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	      searchTerm.addBeforeSearch(date);
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
	    searchTerm.addBodySearch((char *)&m_parseBuffer[tokenPointer]);
	    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	  }
	  else {
	    result = false;
	  }
	  break;

	case SSV_CC:
	  if (tokenPointer < m_parsePointer) {
	    searchTerm.addCcSearch((char *)&m_parseBuffer[tokenPointer]);
	    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	  }
	  else {
	    result = false;
	  }
	  break;

	case SSV_DELETED:
	  searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_DELETED);
	  break;

	case SSV_FLAGGED:
	  searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_FLAGGED);
	  break;

	case SSV_FROM:
	  if (tokenPointer < m_parsePointer) {
	    searchTerm.addFromSearch((char *)&m_parseBuffer[tokenPointer]);
	    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	  }
	  else {
	    result = false;
	  }
	  break;

	case SSV_KEYWORD:
	  // Searching for this cannot return any matches because we don't have keywords to set
	  searchTerm.forceNoMatches();
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
	  searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_SEEN);
	  searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_RECENT);
	  break;

	case SSV_OLD:
	  searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_RECENT);
	  break;

	case SSV_ON:
	  if (tokenPointer < m_parsePointer) {
	    std::string dateString((char *)&m_parseBuffer[tokenPointer]);
	    // SYZYGY -- need a try-catch here
	    // DateTime date(dateString.c_str()); 
	    DateTime date;
	    // date.LooseDate(dateString);
	    if (date.isValid()) {
	      tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	      searchTerm.addOnSearch(date);
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
	  searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_RECENT);
	  break;

	case SSV_SEEN:
	  searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_SEEN);
	  break;

	case SSV_SINCE:
	  if (tokenPointer < m_parsePointer) {
	    std::string dateString((char *)&m_parseBuffer[tokenPointer]);	
	    // SYZYGY -- need a try-catch here
	    // DateTime date(dateString);
	    DateTime date;
	    // date.LooseDate(dateString);
	    if (date.isValid()) {
	      tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	      searchTerm.addSinceSearch(date);
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
	    searchTerm.addSubjectSearch((char *)&m_parseBuffer[tokenPointer]);
	    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	  }
	  else {
	    result = false;
	  }
	  break;

	case SSV_TEXT:
	  if (tokenPointer < m_parsePointer) {
	    searchTerm.addTextSearch((char *)&m_parseBuffer[tokenPointer]);
	    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	  }
	  else {
	    result = false;
	  }
	  break;

	case SSV_TO:
	  if (tokenPointer < m_parsePointer) {
	    searchTerm.addToSearch((char *)&m_parseBuffer[tokenPointer]);
	    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	  }
	  else {
	    result = false;
	  }
	  break;

	case SSV_UNANSWERED:
	  searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_ANSWERED);
	  break;

	case SSV_UNDELETED:
	  searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_DELETED);
	  break;

	case SSV_UNFLAGGED:
	  searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_FLAGGED);
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
	  searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_SEEN);
	  break;

	case SSV_DRAFT:
	  searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_DRAFT);
	  break;

	case SSV_HEADER:
	  if (tokenPointer < m_parsePointer) {
	    char *header_name = (char *)&m_parseBuffer[tokenPointer];
	    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	    if (tokenPointer < m_parsePointer) {
	      searchTerm.addGenericHeaderSearch(header_name, (char *)&m_parseBuffer[tokenPointer]);
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
	      searchTerm.addSmallestSize(value + 1);
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
	    if (updateSearchTerm(termToBeNotted, tokenPointer)) {
	      SEARCH_RESULT vector;
	      // messageList will not return CANNOT_COMPLETE here since I locked the mail store in the caller
	      if (MailStore::SUCCESS ==  m_store->messageList(vector)) {
		if (MailStore::SUCCESS == termToBeNotted.evaluate(m_store)) {
		  SEARCH_RESULT rightResult;
		  termToBeNotted.reportResults(m_store, &rightResult);

		  for (SEARCH_RESULT::iterator i=rightResult.begin(); i!=rightResult.end(); ++i) {
		    SEARCH_RESULT::iterator elem = find(vector.begin(), vector.end(), *i);
		    if (vector.end() != elem) {
		      vector.erase(elem);
		    }
		  }
		  searchTerm.addUidVector(vector);
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
	      if (updateSearchTerm(termLeftSide, tokenPointer)) {
		if (MailStore::SUCCESS == termLeftSide.evaluate(m_store)) {
		  MailSearch termRightSide;
		  if (updateSearchTerm(termRightSide, tokenPointer)) {
		    if (MailStore::SUCCESS == termRightSide.evaluate(m_store)) {
		      SEARCH_RESULT leftResult;
		      termLeftSide.reportResults(m_store, &leftResult);
		      SEARCH_RESULT rightResult;
		      termRightSide.reportResults(m_store, &rightResult);
		      for (int i=0; i<rightResult.size(); ++i) {
			unsigned long uid = rightResult[i];
			int which;

			if (leftResult.end() == find(leftResult.begin(), leftResult.end(), uid)) {
			  leftResult.push_back(uid);
			}
		      }
		      searchTerm.addUidVector(leftResult);
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
	    if (date.isValid()) {
	      tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	      searchTerm.addSentBeforeSearch(date);
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
	    if (date.isValid()) {
	      tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	      searchTerm.addSentOnSearch(date);
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
	    if (date.isValid()) {
	      tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	      searchTerm.addSentSinceSearch(date);
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
	      searchTerm.addLargestSize(value - 1);
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

	    result = uidSequenceSet(srVector, tokenPointer);
	    searchTerm.addUidVector(srVector);
	    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	  }
	  else {
	    result = false;
	  }
	  break;

	case SSV_UNDRAFT:
	  searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_DRAFT);
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

// updateSearchTerms calls updateSearchTerm until there is an error, it reaches a ")" or it reaches the end of 
// the string.  If there is an error, indicated by updateSearchTerm returning false, or if it reaches a ")" and
// isSubExpression is not true, then false is returned.  Otherwise, true is returned.  
bool ImapSession::updateSearchTerms(MailSearch &searchTerm, size_t &tokenPointer, bool isSubExpression) {
  bool notdone = true;
  bool result;
  do {
    result = updateSearchTerm(searchTerm, tokenPointer);
  } while(result && (tokenPointer < m_parsePointer) && (')' != m_parseBuffer[tokenPointer]));
  if ((')' == m_parseBuffer[tokenPointer]) && !isSubExpression) {
    result = false;
  }
  if (')' == m_parseBuffer[tokenPointer]) {
    tokenPointer += 2;
  }
  return result;
}

// searchHandlerExecute assumes that the tokens for the search are in m_parseBuffer from 0 to m_parsePointer and
// that they are stored there as a sequence of ASCIIZ strings.  It is also known that when the system gets
// here that it's time to generate the output
IMAP_RESULTS ImapSession::searchHandlerExecute(bool usingUid) {
  SEARCH_RESULT foundVector;
  NUMBER_LIST foundList;
  IMAP_RESULTS result;
  MailSearch searchTerm;
  bool hasNoMatches = false;
  size_t executePointer = 0;

  if (MailStore::SUCCESS == m_store->lock()) {
    // Skip the first two tokens that are the tag and the command
    executePointer += (size_t) strlen((char *)m_parseBuffer) + 1;
    executePointer += (size_t) strlen((char *)&m_parseBuffer[executePointer]) + 1;

    // I do this once again if bUsingUid is set because the command in that case is UID
    if (usingUid) {
      executePointer += (size_t) strlen((char *)&m_parseBuffer[executePointer]) + 1;
    }

    // This section turns the search spec into something that might work efficiently
    if (updateSearchTerms(searchTerm, executePointer, false)) {
      m_mboxErrorCode = searchTerm.evaluate(m_store);
      searchTerm.reportResults(m_store, &foundVector);

      // This section turns the vector of UID numbers into a list of either UID numbers or MSN numbers
      // whichever I'm supposed to be displaying.  If I were putting the numbers into some sort of 
      // order, this is where that would happen
      if (!usingUid) {
	for (int i=0; i<foundVector.size(); ++i) {
	  if (0 != foundVector[i]) {
	    foundList.push_back(m_store->mailboxUidToMsn(foundVector[i]));
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
	m_driver->wantsToSend(ss.str());
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
    m_store->unlock();
  }
  else {
    result = IMAP_TRY_AGAIN;
  }

  return result;
}

IMAP_RESULTS ImapSession::searchKeyParse(uint8_t *data, size_t dataLen, size_t &parsingAt) {
  IMAP_RESULTS result = IMAP_OK;
  while((IMAP_OK == result) && (dataLen > parsingAt)) {
    if (' ' == data[parsingAt]) {
      ++parsingAt;
    }
    if ('(' == data[parsingAt]) {
      addToParseBuffer(&data[parsingAt], 1);
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
	  addToParseBuffer(&data[parsingAt], len);
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
	  addToParseBuffer(&data[parsingAt], len);
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
      addToParseBuffer(&data[parsingAt], 1);
      ++parsingAt;
    }
  }
  if (IMAP_OK == result) {
    m_parseStage = 3;
    result = searchHandlerExecute(m_usesUid);
  }
  return result;
}


IMAP_RESULTS ImapSession::searchHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool usingUid) {
  IMAP_RESULTS result = IMAP_OK;

  m_usesUid = usingUid;
  size_t firstArgOffset = m_parsePointer;
  size_t savedParsePointer = parsingAt;
  switch (m_parseStage) {
  case 0:
    if (('(' == data[parsingAt]) || ('*' == data[parsingAt])) {
      m_parsePointer = firstArgOffset;
      parsingAt = savedParsePointer;
      result = searchKeyParse(data, dataLen, parsingAt);
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
	      result = searchKeyParse(data, dataLen, parsingAt);
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
	result = searchKeyParse(data, dataLen, parsingAt);
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
	result = searchKeyParse(data, dataLen, i);
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
      addToParseBuffer(data, m_literalLength);
      size_t i = m_literalLength;
      m_literalLength = 0;
      if (2 < dataLen) {
	// Get rid of the CRLF if I have it
	dataLen -= 2;
	data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
      }
      result = searchKeyParse(data, dataLen, i);
    }
    else {
      result = IMAP_IN_LITERAL;
      addToParseBuffer(data, dataLen, false);
      m_literalLength -= dataLen;
    }
    break;

  default:
    strncpy(m_responseText, "Completed", MAX_RESPONSE_STRING_LENGTH);
    result = searchHandlerExecute(m_usesUid);
    break;
  }
  return result;
}

// removeRfc822Comments assumes that the lines have already been unfolded
static insensitiveString removeRfc822Comments(const insensitiveString &headerLine) {
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

void ImapSession::messageChunk(unsigned long uid, size_t offset, size_t length) {
  if (MailStore::SUCCESS == m_store->openMessageFile(uid)) {
    char *xmitBuffer = new char[length+1];
    size_t charsRead = m_store->readMessage(xmitBuffer, offset, length);
    m_store->closeMessageFile();
    xmitBuffer[charsRead] = '\0';

    std::ostringstream literal;
    literal << "{" << charsRead << "}\r\n";
    m_driver->wantsToSend(literal.str());
    m_driver->wantsToSend((uint8_t *)xmitBuffer, charsRead);
    delete[] xmitBuffer;
  }
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

void ImapSession::fetchResponseInternalDate(const MailMessage *message) {
  std::string result = "INTERNALDATE \"";
  DateTime when = m_store->messageInternalDate(message->uid());
  when.format(DateTime::IMAP);

  result += when.str();
  result += "\"";
  m_driver->wantsToSend(result);
}

void ImapSession::fetchResponseRfc822(unsigned long uid, const MailMessage *message) {
  m_driver->wantsToSend("RFC822 ");
  messageChunk(uid, 0, message->messageBody().bodyOctets);
}

void ImapSession::fetchResponseRfc822Header(unsigned long uid, const MailMessage *message) {
  size_t length;
  if (0 != message->messageBody().headerOctets) {
    length = message->messageBody().headerOctets;
  }
  else {
    length = message->messageBody().bodyOctets;
  }
  m_driver->wantsToSend("RFC822.HEADER ");
  messageChunk(uid, 0, length);
}

void ImapSession::fetchResponseRfc822Size(const MailMessage *message) {
  std::ostringstream result;
  result << "RFC822.SIZE " << message->messageBody().bodyOctets;
  m_driver->wantsToSend(result.str());
}

void ImapSession::fetchResponseRfc822Text(unsigned long uid, const MailMessage *message) {
  std::string result;
  if (0 < message->messageBody().headerOctets) {
    size_t length;
    if (message->messageBody().bodyOctets > message->messageBody().headerOctets) {
      length = message->messageBody().bodyOctets - message->messageBody().headerOctets;
    }
    else {
      length = 0;
    }
    m_driver->wantsToSend("RFC822.TEXT ");
    messageChunk(uid, message->messageBody().headerOctets, length);
  }
  else {
    m_driver->wantsToSend("RFC822.TEXT {0}\r\n");
  }
}

static insensitiveString quotifyString(const insensitiveString &input) {
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

static insensitiveString rfc822DotAtom(insensitiveString &input) {
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


static insensitiveString parseRfc822Domain(insensitiveString &input) {
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
    insensitiveString token = rfc822DotAtom(input);
    result += quotifyString(token);
  }
  return result;
}

static insensitiveString parseRfc822AddrSpec(insensitiveString &input) {
  insensitiveString result;

  if ('@' != input[0]) {
    result += "NIL ";
  }
  else {
    // It's an old literal path
    int pos = input.find(':');
    if (std::string::npos != pos) {
      insensitiveString temp = quotifyString(input.substr(0, pos));
      int begin = temp.find_first_not_of(SPACE);
      int end = temp.find_last_not_of(SPACE, pos-1);
      result += temp.substr(begin, end-begin+1) + " ";
      input = input.substr(pos+1);
    }
    else {
      result += "NIL ";
    }
  }
  insensitiveString token = rfc822DotAtom(input);
  result += quotifyString(token) + " ";
  if ('@' == input[0]) {
    input = input.substr(1);
    result += parseRfc822Domain(input);
  }
  else {
    result += "\"\"";
  }
  return result;
}

static insensitiveString parseMailbox(insensitiveString &input) {
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
    result += parseRfc822AddrSpec(input);
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
      result += "NIL " + parseRfc822AddrSpec(input);
    }
    else {
      insensitiveString token = rfc822DotAtom(input);
      begin = input.find_first_not_of(SPACE);
      end = input.find_last_not_of(SPACE);
      input = input.substr(begin, end-begin+1);

      // At this point, I don't know if I've seen a local part or an address_spec
      // I can tell the difference by the first character.  If it's a at sign or if I'm out of string,
      // then it's a local part  If it's a greater than, then it was a display name and I'm expecting
      // an address
      if ((0 == input.size()) || (',' == input[0])) {
	result += "NIL NIL " + quotifyString(token) + " \"\"";
      }
      else {
	if ('@' == input[0]) {
	  input = input.substr(1);
	  result += "NIL NIL " + quotifyString(token) + " " + parseRfc822Domain(input);
	}
	else {
	  if ('<' == input[0]) {
	    input = input.substr(1);
	  }
	  result += quotifyString(token) + " " + parseRfc822AddrSpec(input);
	}
      }
    }
  }
  result += ")";
  return result;
}

static insensitiveString parseMailboxList(const insensitiveString &input) {
  insensitiveString result;

  if (0 != input.size()) {
    result = "(";
    insensitiveString work = removeRfc822Comments(input);
    do {
      if (',' == work[0]) {
	work = work.substr(1);
      }
      result += parseMailbox(work);
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
static insensitiveString parseAddress(insensitiveString &input) {
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
    result += parseRfc822AddrSpec(input);
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
      result += "NIL " + parseRfc822AddrSpec(input);
    }
    else {
      insensitiveString token = rfc822DotAtom(input);

      end = input.find_last_not_of(SPACE);
      begin = input.find_first_not_of(SPACE);
      input = input.substr(begin, end-begin+1);

      // At this point, I don't know if I've seen a local part, a display name, or an address_spec
      // I can tell the difference by the first character.  If it's a at sign or if I'm out of string,
      // then it's a local part  If it's a colon, then it was a display name and I'm reading a group.
      //  If it's a greater than, then it was a display name and I'm expecting an address
      if ((0 == input.size()) || (',' == input[0])) {
	result += "NIL NIL " + quotifyString(token) + " \"\"";
      }
      else {
	if ('@' == input[0]) {
	  input = input.substr(1);
	  insensitiveString domain = parseRfc822Domain(input);
	  if ('(' == input[0]) {
	    input = input.substr(1, input.find(')')-1);
	    result += quotifyString(input) + " NIL " + quotifyString(token) + " " + domain;
	  }
	  else {
	    result += "NIL NIL " + quotifyString(token) + " " + domain;
	  }
	  // SYZYGY -- 
	  // SYZYGY -- I wonder what I forgot in the previous line
	}
	else {
	  if ('<' == input[0]) {
	    input = input.substr(1);
	    result += quotifyString(token) + " " + parseRfc822AddrSpec(input);
	  }
	  else {
	    if (':' == input[0]) {
	      input = input.substr(1);
	      // It's a group specification.  I mark the start of the group with (NIL NIL <token> NIL)
	      result += "NIL NIL " + quotifyString(token) + " NIL)";
	      // The middle is a mailbox list
	      result += parseMailboxList(input);
	      // I mark the end of the group with (NIL NIL NIL NIL)
	      if (';' == input[0]) {
		input = input.substr(1);
		result += "(NIL NIL NIL NIL";
	      }
	    }
	    else {
	      result += quotifyString(token) + " " + parseRfc822AddrSpec(input);
	    }
	  }
	}
      }
    }
  }
  result += ")";
  return result;
}

static insensitiveString parseAddressList(const insensitiveString &input) {
  insensitiveString result;

  if (0 != input.size()) {
    result = "(";
    insensitiveString work = input; // removeRfc822Comments(input);
    do {
      if (',' == work[0]) {
	work = work.substr(1);
      }
      result += parseAddress(work);
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

// fetchResponseEnvelope returns the envelope data of the message.
// The envelope data is a parenthesized list with the following fields
// in this order:
// date (string -- taken from m_csDateLine), subject (string -- taken from m_csSubject),
// from (parenthesized list of addresses from m_csFromLine), sender (parenthesized list
// of addresses from m_csSenderLine), reply-to (parenthesized list of addresses from 
// m_csReplyToLine), to (parenthesized list of addresses from m_csToLine), cc (parenthesized
// list of addresses from m_csCcLine), bcc (parenthesized list of addresses from m_csBccLine)
// in-reply-to (string from m_csInReplyTo), and message-id (string from m_csMessageId)
void ImapSession::fetchResponseEnvelope(const MailMessage *message) {
  insensitiveString result("ENVELOPE ("); 
  result += quotifyString(message->dateLine()) + " ";
  if (0 != message->subject().size()) {
    result += quotifyString(message->subject()) + " ";
  }
  else {
    result += "NIL ";
  }
  insensitiveString from = parseAddressList(message->from()) + " ";
  result += from;
  if (0 != message->sender().size()) {
    result += parseAddressList(message->sender()) + " ";
  }
  else {
    result += from;
  }
  if (0 != message->replyTo().size()) {
    result += parseAddressList(message->replyTo()) + " ";
  }
  else {
    result += from;
  }
  result += parseAddressList(message->to()) + " ";
  result += parseAddressList(message->cc()) + " ";
  result += parseAddressList(message->bcc()) + " ";
  if (0 != message->inReplyTo().size()) {
    result += quotifyString(message->inReplyTo()) + " ";
  }
  else
    {
      result += "NIL ";
    }
  if (0 != message->messageId().size()) {
    result += quotifyString(message->messageId());
  }
  else {
    result += "NIL ";
  }
  result += ")";
  m_driver->wantsToSend(result);
}

// The part before the slash is the body type
// The default type is "TEXT"
static insensitiveString parseBodyType(const insensitiveString &typeLine) {	
  insensitiveString result = removeRfc822Comments(typeLine);
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
static insensitiveString parseBodySubtype(const insensitiveString &typeLine, MIME_MEDIA_TYPES type) {
  insensitiveString result = removeRfc822Comments(typeLine);
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
static insensitiveString parseBodyParameters(const insensitiveString &typeLine, MIME_MEDIA_TYPES type) {
  insensitiveString uncommented = removeRfc822Comments(typeLine);
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

// fetchSubpartEnvelope returns the envelope data of an RFC 822 message subpart.
// The envelope data is a parenthesized list with the following fields
// in this order:
// date (string -- taken from m_csDateLine), subject (string -- taken from m_subject),
// from (parenthesized list of addresses from m_fromLine), sender (parenthesized list
// of addresses from m_senderLine), reply-to (parenthesized list of addresses from
// m_csReplyToLine), to (parenthesized list of addresses from m_csToLine), cc (parenthesized
// list of addresses from m_csCcLine), bcc (parenthesized list of addresses from m_csBccLine)
// in-reply-to (string from m_csInReplyTo), and message-id (string from m_csMessageId)
static insensitiveString fetchSubpartEnvelope(const MESSAGE_BODY &body) {
  insensitiveString result("(");
  HEADER_FIELDS::const_iterator field = body.fieldList.find("date");
  if (body.fieldList.end() != field) {
    result += quotifyString(field->second.c_str()) + " ";
  }
  else {
    result += "NIL ";
  }
  field = body.fieldList.find("subject");
  if (body.fieldList.end() != field) {
    result += quotifyString(field->second.c_str()) + " ";
  }
  else {
    result += "NIL ";
  }
  field = body.fieldList.find("from");
  insensitiveString from;
  if (body.fieldList.end() != field) {
    from = parseAddressList(field->second.c_str()) + " ";
  }
  else {
    from = "NIL ";
  }
  result += from;
  field = body.fieldList.find("sender");
  if (body.fieldList.end() != field) {
    result += parseAddressList(field->second.c_str()) + " ";
  }
  else {
    result += from;
  }
  field = body.fieldList.find("reply-to");
  if (body.fieldList.end() != field) {
    result += parseAddressList(field->second.c_str()) + " ";
  }
  else {
    result += from;
  }
  field = body.fieldList.find("to");
  if (body.fieldList.end() != field) {
    result += parseAddressList(field->second.c_str()) + " ";
  }
  else {
    result += "NIL ";
  }
  field = body.fieldList.find("cc");
  if (body.fieldList.end() != field) {
    result += parseAddressList(field->second.c_str()) + " ";
  }
  else {
    result += "NIL ";
  }
  field = body.fieldList.find("bcc");
  if (body.fieldList.end() != field) {
    result += parseAddressList(field->second.c_str()) + " ";
  }
  else {
    result += "NIL ";
  }
  field = body.fieldList.find("in-reply-to");
  if (body.fieldList.end() != field) {
    result += quotifyString(field->second.c_str()) + " ";
  }
  else {
    result += "NIL ";
  }
  field = body.fieldList.find("message-id");
  if (body.fieldList.end() != field) {
    result += quotifyString(field->second.c_str());
  }
  else {
    result += "NIL";
  }
  result += ")";
  return result;
}

// fetchResponseBodyStructure builds a bodystructure response.  The body structure is a representation
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
static std::string fetchResponseBodyStructureHelper(const MESSAGE_BODY &body, bool includeExtensionData) {
  std::ostringstream result;
  result << "(";
  switch(body.bodyMediaType) {
  case MIME_TYPE_MULTIPART:
    if (NULL != body.subparts) {
      for (int i=0; i<body.subparts->size(); ++i) {
	MESSAGE_BODY part = (*body.subparts)[i];
	result << fetchResponseBodyStructureHelper(part, includeExtensionData);
      }
    }
    result << " " << parseBodySubtype(body.contentTypeLine, body.bodyMediaType).c_str();
    break;

  case MIME_TYPE_MESSAGE:
    result << parseBodyType(body.contentTypeLine).c_str();
    result << " " << parseBodySubtype(body.contentTypeLine, body.bodyMediaType).c_str(); {
      insensitiveString uncommented = removeRfc822Comments(body.contentTypeLine);
      int pos = uncommented.find(';');
      if (std::string::npos == pos) {
	result << " NIL";
      }
      else {
	result << " " << parseBodyParameters(body.contentTypeLine, body.bodyMediaType).c_str();
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
      result << " " << fetchSubpartEnvelope(part).c_str();
      result << " " << fetchResponseBodyStructureHelper(part, includeExtensionData);
    }
    break;

  default:
    result << parseBodyType(body.contentTypeLine).c_str();
    result << " " << parseBodySubtype(body.contentTypeLine, body.bodyMediaType).c_str();
    result << " " << parseBodyParameters(body.contentTypeLine, body.bodyMediaType).c_str();
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

void ImapSession::fetchResponseBodyStructure(const MailMessage *message) {
  std::string result("BODYSTRUCTURE ");
  result +=  fetchResponseBodyStructureHelper(message->messageBody(), true);
  m_driver->wantsToSend(result);
}

void ImapSession::fetchResponseBody(const MailMessage *message) {
  std::string result("BODY ");
  result +=  fetchResponseBodyStructureHelper(message->messageBody(), false);
  m_driver->wantsToSend(result);
}

void ImapSession::fetchResponseUid(unsigned long uid) {
  std::ostringstream result;
  result << "UID " << uid;
  m_driver->wantsToSend(result.str());
}

#define SendBlank() (blankLen?(m_driver->wantsToSend(" ",1),0):0)

IMAP_RESULTS ImapSession::fetchHandlerExecute(bool usingUid) {
  IMAP_RESULTS finalResult = IMAP_OK;

  size_t executePointer = strlen((char *)m_parseBuffer) + 1;
  executePointer += strlen((char *)&m_parseBuffer[executePointer]) + 1;
  SEARCH_RESULT srVector;

  if (usingUid) {
    executePointer += strlen((char *)&m_parseBuffer[executePointer]) + 1;
    uidSequenceSet(srVector, executePointer);
  }
  else {
    if (!msnSequenceSet(srVector, executePointer)) {
      strncpy(m_responseText, "Invalid Message Sequence Number", MAX_RESPONSE_STRING_LENGTH);
      finalResult = IMAP_NO;
    }
  }

  if (MailStore::SUCCESS == m_store->lock()) {
    for (int i = 0; (IMAP_BAD != finalResult) && (i < srVector.size()); ++i) {
      int blankLen = 0;
      MailMessage *message;

      MailMessage::MAIL_MESSAGE_RESULT messageReadResult = m_store->messageData(&message, srVector[i]);
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
	      else {
		strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		result = IMAP_BAD;
	      }
	    }
	    else {
	      strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	      result = IMAP_BAD;
	    }
	    MESSAGE_BODY body = message->messageBody();
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
	  fetchResult << "* " << message->msn() << " FETCH (";
	  m_driver->wantsToSend(fetchResult.str());
	}
	while ((IMAP_OK == result) && (specificationBase < m_parsePointer)) {
	  SendBlank();
	  FETCH_NAME_T::iterator which = fetchSymbolTable.find((char *)&m_parseBuffer[specificationBase]);
	  if (fetchSymbolTable.end() != which) {
	    blankLen = 1;
	    switch(which->second) {
	    case FETCH_ALL:
	      fetchResponseFlags(message->messageFlags());
	      SendBlank();
	      fetchResponseInternalDate(message);
	      SendBlank();
	      fetchResponseRfc822Size(message);
	      SendBlank();
	      fetchResponseEnvelope(message);
	      break;

	    case FETCH_FAST:
	      fetchResponseFlags(message->messageFlags());
	      SendBlank();
	      fetchResponseInternalDate(message);
	      SendBlank();
	      fetchResponseRfc822Size(message);
	      break;

	    case FETCH_FULL:
	      fetchResponseFlags(message->messageFlags());
	      SendBlank();
	      fetchResponseInternalDate(message);
	      SendBlank();
	      fetchResponseRfc822Size(message);
	      SendBlank();
	      fetchResponseEnvelope(message);
	      SendBlank();
	      fetchResponseBody(message);
	      break;

	    case FETCH_BODY_PEEK:
	    case FETCH_BODY:
	      fetchResponseBody(message);
	      break;

	    case FETCH_BODYSTRUCTURE:
	      fetchResponseBodyStructure(message);
	      break;

	    case FETCH_ENVELOPE:
	      fetchResponseEnvelope(message);
	      break;

	    case FETCH_FLAGS:
	      fetchResponseFlags(message->messageFlags());
	      break;

	    case FETCH_INTERNALDATE:
	      fetchResponseInternalDate(message);
	      break;

	    case FETCH_RFC822:
	      fetchResponseRfc822(uid, message);
	      seenFlag = true;
	      break;
						
	    case FETCH_RFC822_HEADER:
	      fetchResponseRfc822Header(uid, message);
	      break;

	    case FETCH_RFC822_SIZE:
	      fetchResponseRfc822Size(message);
	      break;

	    case FETCH_RFC822_TEXT:
	      fetchResponseRfc822Text(uid, message);
	      seenFlag = true;
	      break;

	    case FETCH_UID:
	      fetchResponseUid(uid);
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
		  m_driver->wantsToSend("BODY[");
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
	    MESSAGE_BODY body = message->messageBody();
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
		  m_driver->wantsToSend(sectionString.str());
		  body = (*body.subparts)[section-1];
		  specificationBase += (end - ((char *)&m_parseBuffer[specificationBase]));
		  if ('.' == *end) {
		    m_driver->wantsToSend(".");
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
		m_driver->wantsToSend(sectionString.str());
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
		    m_driver->wantsToSend("HEADER");
		    whichPart = FETCH_BODY_HEADER;
		  }
		  else {
		    if (part.substr(6,7) == ".FIELDS") {
		      if (13 == part.size()) {
			specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
			m_driver->wantsToSend("HEADER.FIELDS");
			whichPart = FETCH_BODY_FIELDS;
		      }
		      else {
			if (part.substr(13) == ".NOT") {
			  specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
			  m_driver->wantsToSend("HEADER.FIELDS.NOT");
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
		    m_driver->wantsToSend("TEXT");
		    whichPart = FETCH_BODY_TEXT;
		  }
		  else {
		    if (part.substr(0, 5) == "MIME]") {
		      specificationBase += 4;
		      m_driver->wantsToSend("MIME");
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
		m_driver->wantsToSend(" (");
		specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
		int blankLen = 0;
		while((IMAP_BUFFER_LEN > specificationBase) &&
		      (0 != strcmp((char *)&m_parseBuffer[specificationBase], ")"))) {
		  int len = (int) strlen((char *)&m_parseBuffer[specificationBase]);
		  SendBlank();
		  m_driver->wantsToSend(&m_parseBuffer[specificationBase], len);
		  blankLen = 1;
		  fieldList.push_back((char*)&m_parseBuffer[specificationBase]);
		  specificationBase += len + 1;
		}
		if (0 == strcmp((char *)&m_parseBuffer[specificationBase], ")")) {
		  m_driver->wantsToSend(")");
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
	      m_driver->wantsToSend("]");
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
		m_driver->wantsToSend(limit.str());
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
		  m_driver->wantsToSend(" ");
		  messageChunk(uid, firstByte + body.bodyStartOffset, length);
		}
		else {
		  m_driver->wantsToSend(" {0}\r\n");
		}
		break;

	      case FETCH_BODY_HEADER:
		if (!partNumberFlag || ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size()))) {
		  if (partNumberFlag) {
		    // It's a subpart
		    body = (*body.subparts)[0];
		  }
		  if (firstByte < body.headerOctets) {
		    length = MIN(body.headerOctets - firstByte, maxLength);
		    m_driver->wantsToSend(" ");
		    messageChunk(uid, firstByte + body.bodyStartOffset, length);
		  }
		  else {
		    m_driver->wantsToSend(" {0}\r\n");
		  }
		}
		else {
		  // If it's not a message subpart, it doesn't have a header.
		  m_driver->wantsToSend(" {0}\r\n");
		}
		break;

	      case FETCH_BODY_MIME:
		if (firstByte < body.headerOctets) {
		  length = MIN(body.headerOctets - firstByte, maxLength);
		  m_driver->wantsToSend(" ");
		  messageChunk(uid, firstByte + body.bodyStartOffset, length);
		}
		else {
		  m_driver->wantsToSend(" {0}\r\n");
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
		  if (firstByte < interesting.size()) {
		    length = MIN(interesting.size() - firstByte, maxLength);

		    std::ostringstream headerFields;

		    headerFields << " {" << length << "}\r\n";
		    headerFields << interesting.substr(firstByte, length);
		    m_driver->wantsToSend(headerFields.str());
		  }
		  else {
		    m_driver->wantsToSend(" {0}\r\n");
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
		    m_driver->wantsToSend(headerFields.str());
		  }
		  else {
		    m_driver->wantsToSend(" {0}\r\n");
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
		    m_driver->wantsToSend(" ");
		    messageChunk(uid, firstByte + body.bodyStartOffset + body.headerOctets, length);
		  }
		  else {
		    m_driver->wantsToSend(" {0}\r\n");
		  }
		}
		else {
		  m_driver->wantsToSend(" {0}\r\n");
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
	  if (seenFlag && (0 == (MailStore::IMAP_MESSAGE_SEEN & message->messageFlags()))) {
	    uint32_t updatedFlags;
	    if (MailStore::SUCCESS == m_store->messageUpdateFlags(srVector[i], ~0, MailStore::IMAP_MESSAGE_SEEN, updatedFlags)) {
	      SendBlank();
	      fetchResponseFlags(updatedFlags);
	      blankLen = 1;
	    }
	  }
	  if (usingUid) {
	    SendBlank();
	    fetchResponseUid(uid);
	    blankLen = 1;
	  }
	  m_driver->wantsToSend(")\r\n");
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
    m_store->unlock();
  }
  else {
    finalResult = IMAP_TRY_AGAIN;
  }
  return finalResult;
}

// The problem is that I'm using m_parseStage to do the parenthesis balance, so this needs to be redesigned
IMAP_RESULTS ImapSession::fetchHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool usingUid) {
  IMAP_RESULTS result = IMAP_OK;

  m_usesUid = usingUid;
  // The first thing up is a sequence set.  This is a sequence of digits, commas, and colons, followed by
  // a space
  switch (m_parseStage) {
  case 0: {
    size_t len = strspn(((char *)data)+parsingAt, "0123456789,:*");
    if ((0 < len) && (dataLen >= (parsingAt + len + 2)) && (' ' == data[parsingAt+len]) && 
	(IMAP_BUFFER_LEN > (m_parsePointer+len+1)))  {
      addToParseBuffer(&data[parsingAt], len);
      parsingAt += len + 1;

      m_parseStage = 1;
      if ('(' == data[parsingAt]) {
	addToParseBuffer(&data[parsingAt], 1);
	++parsingAt;
	++m_parseStage;
	while ((IMAP_OK == result) && (dataLen > parsingAt) && (1 < m_parseStage)) {
	  if((dataLen > parsingAt) && (' ' == data[parsingAt])) {
	    ++parsingAt;
	  }
	  if((dataLen > parsingAt) && ('(' == data[parsingAt])) {
	    addToParseBuffer(&data[parsingAt], 1);
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
	    addToParseBuffer(&data[parsingAt], 1);
	    ++parsingAt;
	    --m_parseStage;
	  }
	}
	if ((IMAP_OK == result) && (1 == m_parseStage)) {
	  if (dataLen == parsingAt) {
	    m_parseStage = 2;
	    result = fetchHandlerExecute(m_usesUid);
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
	    addToParseBuffer(&data[parsingAt], 1);
	    ++parsingAt;
	  }
	  if ((dataLen > parsingAt) && (')' == data[parsingAt])) {
	    addToParseBuffer(&data[parsingAt], 1);
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
	  result = fetchHandlerExecute(m_usesUid);
	}
      }
    }
    else {
      strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
      result = IMAP_BAD;
    }
  }
    break;

  case 1:
    if (dataLen >= m_literalLength) {
      addToParseBuffer(data, m_literalLength);
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
	    addToParseBuffer(&data[i], 1);
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
	    addToParseBuffer(&data[i], 1);
	    ++i;
	  }
	}
	if (IMAP_OK == result) {
	  m_parseStage = 2;
	  result = fetchHandlerExecute(m_usesUid);
	}
      }
      else {
	IMAP_RESULTS result = IMAP_OK;
	while ((IMAP_OK == result) && (dataLen > i) && (')' != data[i]) && (1 < m_parseStage)) {
	  if((dataLen > i) && (' ' == data[i])) {
	    ++i;
	  }
	  if((dataLen > i) && ('(' == data[i])) {
	    addToParseBuffer(&data[i], 1);
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
	    addToParseBuffer(&data[i], 1);
	    ++i;
	    --m_parseStage;
	  }
	}
	if ((IMAP_OK == result) && (1 == m_parseStage)) {
	  if (dataLen == i) {
	    result = fetchHandlerExecute(m_usesUid);
	  }
	  else {
	    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	  }
	}
	if (IMAP_NOTDONE != result) {
	  result = fetchHandlerExecute(m_usesUid);
	}
      }
    }
    else {
      result = IMAP_IN_LITERAL;
      addToParseBuffer(data, dataLen, false);
      m_literalLength -= dataLen;
    }
    break;

  default:
    result = fetchHandlerExecute(m_usesUid);
    break;
  }

  return result;
}

IMAP_RESULTS ImapSession::storeHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool usingUid) {
  IMAP_RESULTS result = IMAP_OK;
  size_t executePointer = m_parsePointer;
  int update = '=';

  while((parsingAt < dataLen) && (' ' != data[parsingAt])) {
    addToParseBuffer(&data[parsingAt++], 1, false);
  }
  addToParseBuffer(NULL, 0);
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
    addToParseBuffer(&temp, 1, false);
  }
  addToParseBuffer(NULL, 0);
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
	addToParseBuffer(NULL, 0);
      }
      else {
	uint8_t temp = toupper(data[parsingAt++]);
	addToParseBuffer(&temp, 1, false);
      }
    }
    else {
      strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
      result = IMAP_BAD;
    }
  }
  addToParseBuffer(NULL, 0);
  if (parsingAt != myDataLen) {
    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
    result = IMAP_BAD;
  }

  if (IMAP_OK == result) {
    SEARCH_RESULT srVector;
    bool sequenceOk;
    if (usingUid) {
      sequenceOk = uidSequenceSet(srVector, executePointer);
    }
    else {
      sequenceOk = msnSequenceSet(srVector, executePointer);
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
		if (MailStore::SUCCESS == m_store->messageUpdateFlags(srVector[i], andMask, orMask, flags)) {
		  if (!silentFlag) {
		    std::ostringstream fetch;
		    fetch << "* " << m_store->mailboxUidToMsn(srVector[i]) << " FETCH (";
		    m_driver->wantsToSend(fetch.str());
		    fetchResponseFlags(flags);
		    if (usingUid) {
		      m_driver->wantsToSend(" ");
		      fetchResponseUid(srVector[i]);
		    }
		    m_driver->wantsToSend(")\r\n");
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

 
IMAP_RESULTS ImapSession::copyHandlerExecute(bool usingUid) {
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
    sequenceOk = uidSequenceSet(vector, execute_pointer);
  }
  else {
    sequenceOk = msnSequenceSet(vector, execute_pointer);
  }
  if (sequenceOk) {
    NUMBER_LIST messageUidsAdded;

    execute_pointer += strlen((char *)&m_parseBuffer[execute_pointer]) + 1;
    std::string mailboxName((char *)&m_parseBuffer[execute_pointer]);
    for (SEARCH_RESULT::iterator i=vector.begin(); (i!=vector.end()) && (IMAP_OK==result); ++i) {
      MailMessage *message;

      MailMessage::MAIL_MESSAGE_RESULT messageReadResult = m_store->messageData(&message, *i);
      if (MailMessage::SUCCESS == messageReadResult) {
	size_t newUid;
	DateTime when = m_store->messageInternalDate(*i);
	uint32_t flags = message->messageFlags();

	char buffer[m_store->bufferLength(*i)+1];
	switch (m_mboxErrorCode = m_store->openMessageFile(*i)) {
	case MailStore::SUCCESS: {
	  size_t size = m_store->readMessage(buffer, 0, m_store->bufferLength(*i));
	  switch (m_mboxErrorCode = m_store->addMessageToMailbox(mailboxName, (uint8_t *)buffer, size, when, flags, &newUid)) {
	  case MailStore::SUCCESS:
	    m_store->doneAppendingDataToMessage(mailboxName, newUid);
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
	m_store->deleteMessage(mailboxName, *i);
      }
    }
  }
  else {
    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
    result = IMAP_BAD;
  }

  return result;
}


IMAP_RESULTS ImapSession::copyHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool usingUid) {
  IMAP_RESULTS result = IMAP_OK;

  if (0 == m_parseStage) {
    while((parsingAt < dataLen) && (' ' != data[parsingAt])) {
      addToParseBuffer(&data[parsingAt++], 1, false);
    }
    addToParseBuffer(NULL, 0);
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
	result = copyHandlerExecute(usingUid);
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
	addToParseBuffer(data, m_literalLength);
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
	copyHandlerExecute(m_usesUid);
      }
    else {
      result = IMAP_IN_LITERAL;
      addToParseBuffer(data, dataLen, false);
      m_literalLength -= dataLen;
    }
  }
  return result;
}


IMAP_RESULTS ImapSession::uidHandler(uint8_t *data, size_t dataLen, size_t &parsingAt, bool flag) {
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
	m_currentHandler = &ImapSession::storeHandler;
	break;

      case UID_FETCH:
	m_currentHandler = &ImapSession::fetchHandler;
	break;

      case UID_SEARCH:
	m_currentHandler = &ImapSession::searchHandler;
	break;

      case UID_COPY:
	m_currentHandler = &ImapSession::copyHandler;
	break;

      default:
	break;
      }
    }
    if (NULL != m_currentHandler) {
      result = (this->*m_currentHandler)(data, dataLen, parsingAt, m_flag);
    }
    else {
      result = unimplementedHandler();
    }
  }
  else {
    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
    result = IMAP_BAD;
  }
  return result;
}
